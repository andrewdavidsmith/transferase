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

#include "lookup.hpp"

#include "client.hpp"
#include "cpg_index.hpp"
#include "genomic_interval.hpp"
#include "logger.hpp"
#include "mc16_error.hpp"
#include "methylome.hpp"
#include "request.hpp"
#include "response.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>  // std::make_shared
#include <print>
#include <ranges>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

using std::cerr;
using std::max;
using std::print;
using std::println;
using std::string;
using std::tuple;
using std::uint32_t;
using std::vector;

namespace rg = std::ranges;
namespace vs = std::views;
using hr_clock = std::chrono::high_resolution_clock;

namespace po = boost::program_options;

template <mc16_log_level the_level, typename... Args>
auto
log_key_value(auto &&key_value_pairs) {
  logger &lgr = logger::instance();
  for (auto &&[k, v] : key_value_pairs)
    lgr.log<the_level>("{}: {}", k, v);
}

[[nodiscard]] static inline auto
do_remote_lookup(const string &accession, const cpg_index &index,
                 vector<methylome::offset_pair> offsets, const string &hostname,
                 const string &port)
  -> tuple<vector<counts_res>, std::error_code> {
  request_header hdr{accession, index.n_cpgs_total, 0};
  request req{static_cast<uint32_t>(size(offsets)), offsets};
  mc16_client mc16c(hostname, port, hdr, req, logger::instance());
  const auto status = mc16c.run();
  if (status) {
    logger::instance().error("Transaction status: {}", status);
    return {{}, status};
  }
  return {std::move(mc16c.take_counts()), {}};
}

[[nodiscard]] static inline auto
do_local_lookup(const string &meth_file, const cpg_index &index,
                vector<methylome::offset_pair> offsets)
  -> tuple<vector<counts_res>, std::error_code> {
  methylome meth{};
  if (const auto meth_read_err = meth.read(meth_file, index.n_cpgs_total)) {
    logger::instance().error("Error: {} ({})", meth_read_err, meth_file);
    return {{}, meth_read_err};
  }
  return {std::move(meth.get_counts(offsets)), {}};
}

static inline auto
write_output(std::ostream &out, const vector<genomic_interval> &gis,
             const cpg_index &index, const vector<counts_res> &results,
             const bool write_scores) {
  if (write_scores) {
    // ADS: counting intervals that have no reads
    uint32_t zero_coverage = 0;
    const auto to_score = [&zero_coverage](const auto &x) {
      zero_coverage += (x.n_meth + x.n_unmeth == 0);
      return x.n_meth / max(1.0, static_cast<double>(x.n_meth + x.n_unmeth));
    };
    write_bedgraph(out, index, gis, vs::transform(results, to_score));
    logger::instance().debug("Number of intervals without reads: {}",
                             zero_coverage);
  }
  else
    write_intervals(out, index, gis, results);
}

auto
lookup_main(int argc, char *argv[]) -> int {
  static constexpr auto usage_format =
    "Usage: mc16 lookup {} [options]\n\nOption groups";
  static constexpr mc16_log_level default_log_level{mc16_log_level::warning};
  static constexpr auto default_port = "5000";
  static const auto command = "lookup";

  bool write_scores{};

  string port{};
  string accession{};
  string index_file{};
  string meth_file{};
  string intervals_file{};
  string hostname{};
  string output_file{};
  mc16_log_level log_level{};

  string subcommand;
  po::options_description subcommands;
  subcommands.add_options()
    // clang-format off
    ("subcommand", po::value(&subcommand))
    ("subargs", po::value<vector<string>>())
    // clang-format on
    ;
  // positional; one for "subcommand" and the rest else parser throws
  po::positional_options_description p;
  p.add("subcommand", 1).add("subargs", -1);

  po::variables_map vm_subcommand;
  po::store(po::command_line_parser(argc, argv)
              .options(subcommands)
              .positional(p)
              .allow_unregistered()
              .run(),
            vm_subcommand);
  po::notify(vm_subcommand);

  if (subcommand != "local" && subcommand != "remote") {
    println(std::cerr, "Usage: mc16 lookup [local|remote] [options]");
    return EXIT_FAILURE;
  }
  const bool remote_mode = subcommand == "remote";

  po::options_description general("General");
  general.add_options()
    // clang-format off
    ("help,h", "produce help message")
    ("index,x", po::value(&index_file)->required(), "index file")
    ("intervals,i", po::value(&intervals_file)->required(), "intervals file")
    ("log-level,l", po::value(&log_level)->default_value(default_log_level),
     "log level {debug,info,warning,error}")
    // clang-format on
    ;
  po::options_description output("Output");
  output.add_options()
    // clang-format off
    ("output,o", po::value(&output_file)->required(), "output file")
    ("score,s", po::bool_switch(&write_scores), "weighted methylation bedgraph format ")
    // clang-format on
    ;
  po::options_description remote("Remote");
  remote.add_options()
    // clang-format off
    ("hostname,H", po::value(&hostname)->required(), "server hostname")
    ("port,p", po::value(&port)->default_value(default_port), "port")
    ("accession,a", po::value(&accession)->required(), "methylome accession")
    // clang-format on
    ;
  po::options_description local("Local");
  local.add_options()
    // clang-format off
    ("methylome,m", po::value(&meth_file)->required(), "local methylome file")
    // clang-format on
    ;

  po::options_description all(std::format(usage_format, subcommand));
  all.add(general).add(output);
  all.add(remote_mode ? remote : local);

  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc - 1, argv + 1, all), vm);
    if (vm.count("help")) {
      all.print(std::cout);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    all.print(cerr);
    return EXIT_FAILURE;
  }

  logger &lgr = logger::instance(
    std::make_shared<std::ostream>(std::cout.rdbuf()), command, log_level);
  if (!lgr) {
    println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  log_key_value<mc16_log_level::info>(vector<tuple<string, string>>{
    // clang-format off
    {"Accession", accession},
    {"Sever", std::format("{}:{}", hostname, port)},
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
    lgr.debug("Number of CpGs in index: {}", index.n_cpgs_total);

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

  const auto lookup_start{hr_clock::now()};
  const auto [results, lookup_err] =
    remote_mode ? do_remote_lookup(accession, index, offsets, hostname, port)
                : do_local_lookup(meth_file, index, offsets);
  const auto lookup_stop{hr_clock::now()};
  lgr.debug("Elapsed time for query: {:.3}s",
            duration(lookup_start, lookup_stop));

  if (lookup_err)  // ADS: error messages already logged
    return EXIT_FAILURE;

  std::ofstream out(output_file);
  if (!out) {
    lgr.error("Failed to open output file: {} ({})", output_file,
              std::make_error_code(std::errc(errno)));
    return EXIT_FAILURE;
  }

  const auto output_start{hr_clock::now()};
  write_output(out, gis, index, results, write_scores);
  const auto output_stop{hr_clock::now()};
  // ADS: elapsed time for output will include conversion to scores
  lgr.debug("Elapsed time for output: {:.3}s",
            duration(output_start, output_stop));

  return EXIT_SUCCESS;
}
