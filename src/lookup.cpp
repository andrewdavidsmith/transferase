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

#include "cpg_index.hpp"
#include "genomic_interval.hpp"
#include "logger.hpp"
#include "mc16_error.hpp"
#include "methylome.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <algorithm>
#include <chrono>
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

auto
lookup_main(int argc, char *argv[]) -> int {
  static constexpr mc16_log_level default_log_level{mc16_log_level::warning};
  static const auto description = "lookup";

  bool write_scores{};

  string index_file{};
  string intervals_file{};
  string meth_file{};
  string output_file{};
  mc16_log_level log_level{};

  po::options_description desc(description);
  desc.add_options()
    // clang-format off
    ("help,h", "produce help message")
    ("index,x", po::value(&index_file)->required(), "index file")
    ("intervals,i", po::value(&intervals_file)->required(), "intervals file")
    ("methylome,m", po::value(&meth_file)->required(), "methylome file")
    ("output,o", po::value(&output_file)->required(), "output file")
    ("score,s", po::bool_switch(&write_scores), "write bedgraph with weighted methylation")
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

  log_key_value<mc16_log_level::info>(vector<tuple<string, string>>{
    // clang-format off
    {"Methylome", meth_file},
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

  methylome meth{};
  const auto meth_read_err = meth.read(meth_file, index.n_cpgs_total);
  if (meth_read_err) {
    println(cerr, "Error: {} ({})", meth_read_err, meth_file);
    return EXIT_FAILURE;
  }
  const auto results = meth.get_counts(offsets);

  const auto lookup_stop{hr_clock::now()};

  lgr.debug("Elapsed time for query: {:.3}s",
            duration(lookup_start, lookup_stop));

  std::ofstream out(output_file);
  if (!out) {
    lgr.error("Failed to open output file: {} ({})", output_file,
              std::make_error_code(std::errc(errno)));
    return EXIT_FAILURE;
  }

  const auto output_start{hr_clock::now()};
  if (write_scores) {
    // ADS: elapsed time for output will include conversion to scores
    // ADS: counting intervals that have no reads
    uint32_t zero_coverage = 0;
    const auto to_score = [&zero_coverage](const auto &x) {
      zero_coverage += (x.n_meth + x.n_unmeth == 0);
      return x.n_meth / max(1.0, static_cast<double>(x.n_meth + x.n_unmeth));
    };
    write_bedgraph(out, index, gis, vs::transform(results, to_score));
  }
  else
    write_intervals(out, index, gis, results);
  const auto output_stop{hr_clock::now()};

  lgr.debug("Elapsed time for output: {:.3}s",
            duration(output_start, output_stop));

  return EXIT_SUCCESS;
}
