/* MIT License
 *
 * Copyright (c) 2024 Andrew D Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "server_sync.hpp"

#include "cpg_index.hpp"
#include "methylome.hpp"

#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include <array>
#include <cassert>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <queue>
#include <ranges>
#include <regex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

using std::array;
using std::cerr;
using std::exception;
using std::make_pair;
using std::ofstream;
using std::ostream;
using std::pair;
using std::print;
using std::println;
using std::ssize;
using std::string;
using std::string_view;
using std::tuple;
using std::uint32_t;
using std::unordered_map;
using std::vector;

namespace asio = boost::asio;
namespace bs = boost::system;
namespace chrono = std::chrono;
namespace fs = std::filesystem;
namespace ip = asio::ip;
namespace po = boost::program_options;
namespace rg = std::ranges;
namespace vs = std::views;

using hr_clock = chrono::high_resolution_clock;
using tcp = ip::tcp;

[[nodiscard]] static inline auto duration(const auto start, const auto stop) {
  return chrono::duration_cast<chrono::duration<double>>(stop - start).count();
}

[[nodiscard]] static auto is_valid_accession(const string &accession) -> bool {
  static constexpr auto experiment_ptrn = R"(^(D|E|S)RX\d+$)";
  std::regex experiment_re(experiment_ptrn);
  return std::regex_search(accession, experiment_re);
}

[[nodiscard]] static auto
read_accession(tcp::socket &socket) -> tuple<string, bs::error_code> {
  // ADS: change to using system errors
  constexpr auto buf_size = 64;
  array<char, buf_size> buf{};
  bs::error_code error;
  // ADS: using transfer_exactly to match the read_accession function
  [[maybe_unused]]
  const auto n = asio::read(socket, asio::buffer(&buf, buf_size),
                            asio::transfer_exactly(buf_size), error);
  return {string(buf.data()), error};
}

template <typename T>
[[nodiscard]] static inline auto write_value(tcp::socket &socket,
                                             T value) -> bs::error_code {
  bs::error_code error;
  [[maybe_unused]]
  const auto n = asio::write(socket, asio::buffer(&value, sizeof(T)),
                             asio::transfer_exactly(sizeof(T)), error);
  assert(n == sizeof(T));
  return error;
}

template <typename T>
[[nodiscard]] static inline auto
read_value(tcp::socket &socket) -> tuple<T, bs::error_code> {
  bs::error_code error;
  T value{};
  [[maybe_unused]]
  const auto n = asio::read(socket, asio::buffer(&value, sizeof(T)),
                            asio::transfer_exactly(sizeof(T)), error);
  return {value, error};
}

template <class T>
[[nodiscard]] static auto
read_vector(tcp::socket &socket, const uint32_t n_items,
            vector<T> &data) -> bs::error_code {
  assert(size(data) >= static_cast<uint64_t>(n_items));

  constexpr auto buf_size = 256 * 1024;
  array<char, buf_size> buf;

  bs::error_code error;
  const auto n_bytes_expected = n_items * sizeof(T);
  uint64_t n_total_bytes{};
#ifdef DEBUG
  uint32_t n_reads{};
#endif
  for (;;) {
    const auto n_bytes_received = socket.read_some(asio::buffer(buf), error);
    if (error)
      break;
    std::memcpy(reinterpret_cast<char *>(data.data()) + n_total_bytes,
                reinterpret_cast<char *>(buf.data()), n_bytes_received);
    n_total_bytes += n_bytes_received;
#ifdef DEBUG
    ++n_reads;
#endif
    if (n_total_bytes == n_bytes_expected)
      break;
  }
#ifdef DEBUG
  println("number of 'read_some' calls: {}", n_reads);
  println("hit eof: {}", error == asio::error::eof);
#endif
  return error;
}

[[nodiscard]] static auto write_vector(tcp::socket &socket,
                                       const auto &data) -> bs::error_code {
  bs::error_code error;
  asio::write(socket, asio::buffer(data), asio::transfer_all(), error);
  return error;
}

template <typename T> struct ring_buffer {
  // queue ops and iterable
  ring_buffer(const uint64_t max_size) :
    max_size{max_size}, counter{0}, buf{max_size} {}
  auto push(T t) -> T {
    std::swap(buf[counter++ % max_size], t);
    return t;
  }
  auto size() const { return counter < max_size ? counter : max_size; }
  auto front() const {
    return buf[counter < max_size ? 0 : (counter + 1) % max_size];
  }

  auto begin() { return begin(buf); }
  auto begin() const { return cbegin(buf); }
  auto end() { return begin(buf) + std::min(counter, max_size); }
  auto end() const { return cbegin(buf) + std::min(counter, max_size); }

  uint64_t max_size{};
  uint64_t counter{};
  vector<T> buf{};
};

struct methylome_set {
  methylome_set(const bool verbose, const uint32_t max_live_methylomes,
                const string &mc16_directory) :
    verbose{verbose}, max_live_methylomes{max_live_methylomes},
    mc16_directory{mc16_directory}, accessions{max_live_methylomes} {}

  [[nodiscard]] auto
  get_methylome(const string &accession) -> tuple<const methylome *, int> {
    if (!is_valid_accession(accession)) {
      if (verbose)
        println(std::cout, "not valid accession: {}", accession);
      return {nullptr, -1};
    }
    if (verbose)
      println(std::cout, "valid accession: {}", accession);

    unordered_map<string, methylome>::const_iterator meth{};

    // check if methylome is loaded
    const auto acc_meth_itr = accession_to_methylome.find(accession);
    if (acc_meth_itr == cend(accession_to_methylome)) {
      if (verbose)
        println(std::cout, "methylome not loaded: {}", accession);
      const auto filename = format("{}/{}.mc16", mc16_directory, accession);
      if (verbose)
        println(std::cout, "filename for methylome: {}", filename);
      if (!fs::exists(filename)) {
        println(std::cout, "file not found: {}", filename);
        return {nullptr, -1};
      }
      const string to_eject = accessions.push(accession);
      if (!to_eject.empty()) {
        const auto to_eject_itr = accession_to_methylome.find(to_eject);
        if (to_eject_itr == cend(accession_to_methylome)) {
          if (verbose)
            println("error ejecting methylome: {}", to_eject);
          return {nullptr, -2};
        }
        accession_to_methylome.erase(to_eject_itr);
      }
      methylome m{};
      if (const auto ec = m.read(filename)) {
        println("error reading methylome file: {} (error={})", filename, ec);
        return {nullptr, -1};
      }
      std::tie(meth, std::ignore) =
        accession_to_methylome.emplace(accession, std::move(m));
    }
    else
      meth = acc_meth_itr;  // already loaded

    return {&meth->second, 0};
  }

  [[nodiscard]] auto summary() const {
    constexpr auto fmt = "n_live_methylomes: {}\n"
                         "max_live_methylomes: {}\n"
                         "mc16_directory: {}\n"
                         "methylomes:";
    string r{
      format(fmt, accessions.size(), max_live_methylomes, mc16_directory)};
    rg::for_each(accessions, [&](const auto &x) { r += format("\n{}", x); });
    return r;
  }

  bool verbose{};
  uint32_t max_live_methylomes{};
  string mc16_directory{};
  ring_buffer<string> accessions;
  unordered_map<string, methylome> accession_to_methylome;
};

auto lookup_server_main(int argc, char *argv[]) -> int {
  static constexpr auto default_max_live_methylomes = 32;
  // static constexpr auto default_log_file = "lookup_server.log";
  static constexpr auto default_port = 5000;
  static constexpr auto description = "server-sync";

  bool verbose{};

  int16_t port{};

  uint32_t max_live_methylomes{};

  string index_file{};
  string mc16_directory{};
  string log_file{};

  po::options_description desc(description);
  // clang-format off
  desc.add_options()
    ("help,h", "produce help message")
    ("index,x", po::value(&index_file)->required(), "index file (consistency check)")
    ("dir,d", po::value(&mc16_directory)->required(), "directory with mc16 files")
    ("port,p", po::value(&port)->default_value(default_port), "port to listen on")
    ("max-live", po::value(&max_live_methylomes)->default_value(default_max_live_methylomes),
     "max methylomes to load simultaneously")
    // ("log,l", po::value(&log_file)->default_value(default_log_file), "log file")
    ("verbose,v", po::bool_switch(&verbose), "print more run info")
    ;
  // clang-format on
  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help")) {
      desc.print(std::cout);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    println(cerr, "{}", e.what());
    desc.print(cerr);
    return EXIT_FAILURE;
  }

  /* ADS: likely will not be implemented here */
  // ofstream logger(log_file, std::ios_base::app);
  // if (!logger) {
  //   println(cerr, "failed to open log file for append: {}", log_file);
  //   return EXIT_FAILURE;
  // }

  if (verbose)
    print(std::cout,
          "index: {}\n"
          "directory: {}\n"
          "log: {}\n",
          index_file, mc16_directory, log_file);

  cpg_index index{};
  if (index.read(index_file) != 0) {
    println(cerr, "failed to read cpg index: {}", index_file);
    return EXIT_FAILURE;
  }

  if (verbose)
    println(std::cout, "index:\n{}", index);

  methylome_set ms(verbose, max_live_methylomes, mc16_directory);
  vector<pair<uint32_t, uint32_t>> intervals;

  asio::io_context io_context{};
  tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v6(), port));

  for (;;) {

    tcp::socket socket(io_context);
    acceptor.accept(socket);

    const auto [accession, accn_ec] = read_accession(socket);
    if (accn_ec) {
      println(std::cout, "{}", accn_ec.message());
      continue;
    }

    // identify the requested methylome
    const auto get_methylome_start{hr_clock::now()};
    const auto [meth, meth_ec] = ms.get_methylome(accession);
    const auto get_methylome_stop{hr_clock::now()};
    if (verbose)
      println(std::cout, "elapsed time for ms.get_methylome: {:.3}s",
              duration(get_methylome_start, get_methylome_stop));
    if (meth_ec) {
      println(std::cout, "error identifying methylome: {}", accession);
      if (const auto ec = write_value(socket, 0))
        println(std::cout, "{}", ec.message());
      continue;
    }

    // respond with methylome size
    const uint32_t methylome_size = size(meth->cpgs);
    if (const auto ec = write_value(socket, methylome_size)) {
      println(std::cout, "{}", ec.message());
      continue;
    }

    // receive the number of intervals incoming
    const auto [n_intervals, n_intervals_ec] = read_value<uint32_t>(socket);
    if (n_intervals_ec) {
      println(std::cout, "{}", n_intervals_ec.message());
      continue;
    }
    if (verbose)
      println(std::cout, "n_intervals: {}", n_intervals);

    // receive the intervals
    intervals.resize(n_intervals);
    if (const auto ec = read_vector(socket, n_intervals, intervals)) {
      println(std::cout, "{}", ec.message());
      continue;
    }

    // compute the results
    const auto lookup_start{hr_clock::now()};
    const auto results = meth->get_counts(intervals);
    const auto lookup_stop{hr_clock::now()};
    if (verbose)
      println(std::cout, "elapsed time for get_counts: {:.3}s",
              duration(lookup_start, lookup_stop));

    // send the results
    if (const auto ec = write_vector(socket, results)) {
      println(std::cout, "{}", ec.message());
      continue;
    }
  }
  return EXIT_SUCCESS;
}
