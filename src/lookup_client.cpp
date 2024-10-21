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

#include "lookup_client.hpp"
#include "client.hpp"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include "cpg_index.hpp"
#include "genomic_interval.hpp"
#include "logger.hpp"
#include "mc16_error.hpp"
#include "methylome.hpp"
#include "request.hpp"
#include "response.hpp"
#include "utilities.hpp"

#include <array>
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>  // std::make_shared
#include <print>
#include <ranges>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using std::array;
using std::cerr;
using std::format;
using std::ofstream;
using std::print;
using std::println;
using std::string;
using std::uint32_t;
using std::vector;

namespace rg = std::ranges;
namespace vs = std::views;
namespace chrono = std::chrono;
using hr_clock = std::chrono::high_resolution_clock;

namespace asio = boost::asio;
namespace po = boost::program_options;

static auto
log_debug_index(const cpg_index &index) {
  static constexpr string delim = "\n";
  logger &lgr = logger::instance();
  for (const auto word : vs::split(format("{}", index), delim))
    lgr.debug("cpg_index: {}", std::string_view(word));
}

template <mc16_log_level the_level, typename... Args>
auto
log_key_value(vector<std::pair<string, string>> &&key_value_pairs) {
  logger &lgr = logger::instance();
  for (auto &&[k, v] : key_value_pairs)
    lgr.log<the_level>("{}: {}", k, v);
}

auto
lookup_client_main(int argc, char *argv[]) -> int {
  static constexpr mc16_log_level default_log_level{mc16_log_level::warning};
  static constexpr auto default_port = "5000";
  static const auto description = "lookup-client";

  string port{};
  string accession{};
  string index_file{};
  string intervals_file{};
  string hostname{};
  string output_file{};
  mc16_log_level log_level{};

  po::options_description desc(description);
  desc.add_options()
    // clang-format off
    ("help,h", "produce help message")
    ("hostname,H", po::value(&hostname)->required(), "hostname")
    ("port,p", po::value(&port)->default_value(default_port), "port")
    ("accession,a", po::value(&accession)->required(), "accession")
    ("index,x", po::value(&index_file)->required(), "index file")
    ("intervals,i", po::value(&intervals_file)->required(), "intervals file")
    ("output,o", po::value(&output_file)->required(), "output file")
    ("log-level", po::value(&log_level)->default_value(default_log_level),
     "log level {debug,info,warning,error}")
    // clang-format on
    ;
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

  logger &lgr = logger::instance(
    std::make_shared<std::ostream>(std::cout.rdbuf()), description, log_level);
  if (!lgr) {
    println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  log_key_value<mc16_log_level::info>({
    // clang-format off
    {"Accession", accession},
    {"Sever", format("{}:{}", hostname, port)},
    {"Index", index_file},
    {"Intervals", intervals_file},
    {"Output", output_file}
    // clang-format on
  });

  cpg_index index{};
  if (const auto index_read_err = index.read(index_file); index_read_err) {
    lgr.error("Failed to read cpg index: {} ({})", index_file, index_read_err);
    return EXIT_FAILURE;
  }

  if (log_level == mc16_log_level::debug)
    log_debug_index(index);

  const auto [gis, ec] = genomic_interval::load(index, intervals_file);
  if (ec) {
    lgr.error("Error reading intervals file: {} ({})", intervals_file, ec);
    return EXIT_FAILURE;
  }
  lgr.info("N intervals: {}", size(gis));

  const auto get_offsets_start{hr_clock::now()};
  const vector<methylome::offset_pair> offsets = index.get_offsets(gis);
  const auto get_offsets_stop{hr_clock::now()};
  lgr.debug("Elapsed time to get offsets: {:.3}s",
            duration(get_offsets_start, get_offsets_stop));

  request_header hdr{accession, index.n_cpgs_total, 0};
  request req{static_cast<uint32_t>(size(offsets)), offsets};

  mc16_client mc16c(hostname, port, hdr, req, lgr);
  const auto client_start{hr_clock::now()};
  const auto status = mc16c.run();
  const auto client_stop{hr_clock::now()};

  lgr.debug("Elapsed time for query: {:.3}s",
            duration(client_start, client_stop));

  if (status) {
    lgr.error("Transaction status: {}", status);
    return EXIT_FAILURE;
  }
  else
    lgr.info("Transaction status: {}", status);

  std::ofstream out(output_file);
  if (!out) {
    lgr.error("Failed to open output file: {} ({})", output_file,
              std::make_error_code(std::errc(errno)));
    return EXIT_FAILURE;
  }

  const auto output_start{hr_clock::now()};
  write_intervals(out, index, gis, mc16c.get_counts());
  const auto output_stop{hr_clock::now()};
  lgr.debug("Elapsed time for output: {:.3}s",
            duration(output_start, output_stop));

  return EXIT_SUCCESS;
}
