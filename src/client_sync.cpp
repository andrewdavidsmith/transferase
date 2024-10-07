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

#include "client_sync.hpp"

#include "cpg_index.hpp"
#include "genomic_interval.hpp"
#include "methylome.hpp"

#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using std::array;
using std::cerr;
using std::make_pair;
using std::ofstream;
using std::pair;
using std::print;
using std::println;
using std::ssize;
using std::string;
using std::string_view;
using std::tuple;
using std::uint32_t;
using std::vector;

namespace asio = boost::asio;
namespace bs = boost::system;
namespace chrono = std::chrono;
namespace ip = boost::asio::ip;
namespace po = boost::program_options;
namespace rg = std::ranges;
namespace vs = std::views;

using hr_clock = chrono::high_resolution_clock;
using tcp = ip::tcp;

// ADS TODO: multiply defined
[[nodiscard]] static inline auto duration(const auto start, const auto stop) {
  return chrono::duration_cast<chrono::duration<double>>(stop - start).count();
}

[[nodiscard]] static auto
write_accession(tcp::socket &socket,
                const string &accession) -> bs::error_code {
  // ADS: change to using boost system errors
  constexpr auto buf_size = 64;
  array<char, buf_size> buf{};
  if (size(accession) > buf_size)
    return bs::errc::make_error_code(bs::errc::value_too_large);
  rg::copy(accession, buf.data());
  bs::error_code error;
  // ADS: using transfer_exactly to match the read_accession function
  asio::write(socket, asio::buffer(buf, buf_size),
              asio::transfer_exactly(buf_size), error);
  return error;
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
[[nodiscard]] static auto read_vector(tcp::socket &socket,
                                      const uint32_t n_items,
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
  println(std::cerr, "number of 'read_some' calls: {}", n_reads);
  println(std::cerr, "hit eof: {}", error == asio::error::eof);
#endif
  return error;
}

[[nodiscard]] static auto write_vector(tcp::socket &socket,
                                       const auto &data) -> bs::error_code {
  bs::error_code error;
  asio::write(socket, asio::buffer(data), asio::transfer_all(), error);
  return error;
}

struct remote_methylome {
  remote_methylome(const string &hostname, const string &port,
                   const bool verbose) : socket(io_context), verbose{verbose} {
    tcp::resolver resolver(io_context);
    const auto resolver_flags = ip::resolver_query_base::flags();
    tcp::resolver::query query(hostname, port, resolver_flags);
    tcp::resolver::results_type endpoints{resolver.resolve(query)};
    asio::connect(socket, endpoints);
  }

  auto lookup(const string &accession,
              const vector<pair<uint32_t, uint32_t>> &offsets)
    -> tuple<vector<counts_res>, int> {
    if (verbose) {
      const auto ipaddr = socket.remote_endpoint().address();
      println(std::cout, "remote ip: {}", ipaddr.to_string());
    }
    if (const auto acn_ec = write_accession(socket, accession)) {
      println(std::cout, "{}", acn_ec.message());
      return {{}, -1};
    }
    const auto [methylome_size, methylome_size_ec] =
      read_value<uint32_t>(socket);
    if (methylome_size_ec) {
      println(std::cout, "{}", methylome_size_ec.message());
      return {{}, -1};
    }
    if (methylome_size == 0) {  // size 0 means methylome unavailable
      println(std::cout, "response of methylome size 0 for: {}", accession);
      return {{}, -1};
    }
    const uint32_t n_intervals = size(offsets);

    // send the size of the intervals
    if (verbose)
      println(std::cout, "sending number of intervals: {}", n_intervals);
    if (const auto ec = write_value(socket, n_intervals)) {
      println(std::cout, "{}", ec.message());
      return {{}, -1};
    }
    // send the intervals themselves
    if (const auto ec = write_vector(socket, offsets)) {
      println(std::cout, "{}", ec.message());
      return {{}, -1};
    }
    // receive the results
    vector<counts_res> results(n_intervals);
    if (const auto ec = read_vector(socket, n_intervals, results)) {
      println(std::cout, "{}", ec.message());
      return {{}, -1};
    }
    return {results, 0};
  }

  asio::io_context io_context{};
  tcp::socket socket;
  bool verbose{};
};

// ADS TODO: multiply defined
static auto
write_intervals(std::ostream &out, const cpg_index &index,
                const vector<genomic_interval> &gis,
                const vector<counts_res> &results) -> void {
  static constexpr auto buf_size{512};

  array<char, buf_size> buf{};
  const auto buf_end = buf.data() + buf_size;

  using gis_res = pair<const genomic_interval &, const counts_res &>;
  const auto same_chrom = [](const gis_res &a, const gis_res &b) {
    return a.first.ch_id == b.first.ch_id;
  };

  for (const auto &chunk : vs::zip(gis, results) | vs::chunk_by(same_chrom)) {
    const auto ch_id = get<0>(chunk.front()).ch_id;
    const string chrom{index.chrom_order[ch_id]};
    rg::copy(chrom, buf.data());
    buf[size(chrom)] = '\t';
    for (const auto &[gi, res] : chunk) {
      std::to_chars_result tcr{buf.data() + size(chrom) + 1, std::errc()};
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wstringop-overflow=0"
      tcr = std::to_chars(tcr.ptr, buf_end, gi.start);
      *tcr.ptr++ = '\t';
      tcr = std::to_chars(tcr.ptr, buf_end, gi.stop);
      *tcr.ptr++ = '\t';
      tcr = std::to_chars(tcr.ptr, buf_end, res.n_meth);
      *tcr.ptr++ = '\t';
      tcr = std::to_chars(tcr.ptr, buf_end, res.n_unmeth);
      *tcr.ptr++ = '\t';
      tcr = std::to_chars(tcr.ptr, buf_end, res.n_covered);
      *tcr.ptr++ = '\n';
#pragma GCC diagnostic push
      out.write(buf.data(), std::distance(buf.data(), tcr.ptr));
    }
  }
}

auto
lookup_client_sync_main(int argc, char *argv[]) -> int {
  static constexpr auto default_port = "5000";
  static const auto description = "client-sync";

  bool verbose{};

  string port{};

  string accession{};
  string index_file{};
  string intervals_file{};
  string hostname{};
  string output_file{};

  po::options_description desc(description);
  // clang-format off
  desc.add_options()
    ("help,h", "produce help message")
    ("hostname,H", po::value(&hostname)->required(), "hostname")
    ("port,p", po::value(&port)->default_value(default_port), "port")
    ("accession,a", po::value(&accession)->required(), "accession")
    ("index,x", po::value(&index_file)->required(), "index file")
    ("intervals,i", po::value(&intervals_file)->required(), "intervals file")
    ("output,o", po::value(&output_file)->required(), "output file")
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

  if (verbose)
    print("accession: {}\n"
          "hostname: {}\n"
          "port: {}\n"
          "index: {}\n"
          "intervals: {}\n"
          "output: {}\n",
          accession, hostname, port, index_file, intervals_file, output_file);

  cpg_index index{};
  if (index.read(index_file) != 0) {
    println(cerr, "failed to read cpg index: {}", index_file);
    return EXIT_FAILURE;
  }

  const auto gis = genomic_interval::load(index, intervals_file);
  if (gis.empty()) {
    println(cerr, "failed to read intervals file: {}", intervals_file);
    return EXIT_FAILURE;
  }

  const auto get_offsets_start{hr_clock::now()};
  const vector<pair<uint32_t, uint32_t>> offsets = index.get_offsets(gis);
  const auto get_offsets_stop{hr_clock::now()};
  if (verbose)
    println("elapsed time for index.get_offsets: {:.3}s",
            duration(get_offsets_start, get_offsets_stop));

  const auto lookup_start{hr_clock::now()};
  remote_methylome rm(hostname, port, verbose);
  const auto [results, lookup_ec] = rm.lookup(accession, offsets);
  const auto lookup_stop{hr_clock::now()};
  if (lookup_ec) {
    println(cerr, "error during remote lookup: {}", lookup_ec);
    return EXIT_FAILURE;
  }
  if (verbose)
    println("elapsed time for remote lookup: {:.3}s",
            duration(lookup_start, lookup_stop));

  ofstream out(output_file);
  if (!out) {
    println(cerr, "failed to open output file: {}", output_file);
    return EXIT_FAILURE;
  }

  const auto output_start{hr_clock::now()};
  write_intervals(out, index, gis, results);
  const auto output_stop{hr_clock::now()};
  if (verbose)
    println("elapsed time for output: {:.3}s",
            duration(output_start, output_stop));

  return EXIT_SUCCESS;
}
