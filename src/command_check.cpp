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

#include "command_check.hpp"

#include "cpg_index.hpp"
#include "logger.hpp"
#include "methylome.hpp"
#include "mxe_error.hpp"

#include <boost/program_options.hpp>

#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

auto
command_check_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "check";

  std::string index_file{};
  std::string methylome_input{};
  std::string metadata_input{};
  std::string output_file{};
  mxe_log_level log_level{};

  namespace po = boost::program_options;

  po::options_description desc(std::format("Usage: mxe {} [options]", command));
  desc.add_options()
    // clang-format off
    ("help,h", "produce help message")
    ("index,x", po::value(&index_file)->required(), "index file")
    ("methylome,m", po::value(&methylome_input)->required(), "methylome file")
    ("meta", po::value(&metadata_input), "methylome metadata file")
    ("output,o", po::value(&output_file)->required(), "output file")
    ("log-level,v", po::value(&log_level)->default_value(logger::default_level),
     "log level {debug,info,warning,error,critical}")
    // clang-format on
    ;
  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help") || argc == 1) {
      desc.print(std::cout);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    std::println("{}", e.what());
    desc.print(std::cout);
    return EXIT_FAILURE;
  }

  logger &lgr = logger::instance(shared_from_cout(), command, log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  if (metadata_input.empty())
    metadata_input = get_default_methylome_metadata_filename(methylome_input);

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Index", index_file},
    {"Methylome", methylome_input},
    {"Metadata", metadata_input},
    {"Output", output_file},
    // clang-format on
  };
  log_args<mxe_log_level::info>(args_to_log);

  const auto [index, cim, index_read_err] = read_cpg_index(index_file);
  if (index_read_err) {
    lgr.error("Failed to read cpg index: {} ({})", index_file, index_read_err);
    return EXIT_FAILURE;
  }

  const auto [meta, meta_read_err] = methylome_metadata::read(metadata_input);
  if (meta_read_err) {
    lgr.error("Error reading metadata {}: {}", metadata_input, meta_read_err);
    return EXIT_FAILURE;
  }

  methylome meth{};
  const auto meth_read_err = meth.read(methylome_input, meta);
  if (meth_read_err) {
    lgr.error("Error reading methylome {}: {}", methylome_input, meth_read_err);
    return EXIT_FAILURE;
  }

  const auto methylome_size = std::size(meth.cpgs);
  const auto check_outcome = (methylome_size == cim.n_cpgs) ? "pass" : "fail";

  const auto total_counts = meth.total_counts_cov();

  std::ofstream out(output_file);
  if (!out) {
    lgr.error("Error opening output file: {} ({})", output_file,
              std::make_error_code(std::errc(errno)));
    return EXIT_FAILURE;
  }

  const double n_reads = total_counts.n_meth + total_counts.n_unmeth;
  const auto mean_meth_weighted = total_counts.n_meth / n_reads;
  const auto sites_covered_fraction =
    static_cast<double>(total_counts.n_covered) / methylome_size;

  std::print(out,
             "check: {}\n"
             "methylome_size: {}\n"
             "index_size: {}\n"
             "total_counts: {}\n"
             "sites_covered_fraction: {}\n"
             "mean_meth_weighted: {}\n",
             check_outcome, methylome_size, cim.n_cpgs, total_counts,
             sites_covered_fraction, mean_meth_weighted);

  return EXIT_SUCCESS;
}
