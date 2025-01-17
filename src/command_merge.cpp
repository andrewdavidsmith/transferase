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

#include "command_merge.hpp"

static constexpr auto about = R"(
merge methylomes
)";

static constexpr auto description = R"(
The merge command takes a set of methylomes and produces a merged
methylome that would be expected if all the data were sequenced
together. One way to understand this function is to think of technical
replicates that are low-coverage and in some analyses might best be
combined as though they were a single methylome. The input methylomes
to be merged must all have been analyzed using the same reference
genome. The output is a methylome: a pair of methylome data (.m16) and
metadata files (.m16.yaml) files.
)";

static constexpr auto examples = R"(
Examples:

xfrase merge -o merged.m16 -i SRX0123*.m16
)";

#include "format_error_code.hpp"  // IWYU pragma: keep
#include "logger.hpp"
#include "methylome.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <chrono>
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <format>
#include <iostream>
#include <iterator>  // for std::size
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>  // IWYU pragma: keep
#include <vector>

auto
command_merge_main(int argc,
                   char *argv[])  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  -> int {
  static constexpr auto command = "merge";
  static const auto usage =
    std::format("Usage: xfrase {} [options]\n", rstrip(command));
  static const auto about_msg =
    std::format("xfrase {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  using transferase::log_args;
  using transferase::log_level_t;
  using transferase::logger;
  using transferase::methylome;
  using transferase::shared_from_cout;

  log_level_t log_level{};
  std::string methylome_directory{};
  std::string methylome_outdir{};
  std::string merged_name{};

  namespace po = boost::program_options;

  po::options_description desc("Options");
  desc.add_options()
    // clang-format off
    ("help,h", "print this message and exit")
    ("methylomes,m", po::value<std::vector<std::string>>()->multitoken()->required(),
     "names of methylomes to merge")
    ("directory,d", po::value(&methylome_directory)->required(), "methylome input directory")
    ("outdir,o", po::value(&methylome_outdir)->required(), "methylome output directory")
    ("name,n", po::value(&merged_name)->required(), "merged methylome name")
    ("log-level,v", po::value(&log_level)->default_value(logger::default_level),
     "{debug, info, warning, error, critical}")
    // clang-format on
    ;
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help") || argc == 1) {
      std::println("{}\n{}", about_msg, usage);
      desc.print(std::cout);
      std::println("\n{}", description_msg);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    std::println("{}", e.what());
    std::println("{}\n{}", about_msg, usage);
    desc.print(std::cout);
    std::println("\n{}", description_msg);
    return EXIT_FAILURE;
  }

  auto &lgr = logger::instance(shared_from_cout(), command, log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  const auto methylome_names = vm["methylomes"].as<std::vector<std::string>>();
  const auto n_methylomes = std::size(methylome_names);

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Output directory", methylome_outdir},
    {"Merged methylome name", merged_name},
    {"Input directory", methylome_directory},
    {"Number of methylomes to merge", std::format("{}", n_methylomes)},
    // clang-format on
  };
  log_args<log_level_t::info>(args_to_log);

  std::vector<std::tuple<std::string, std::string>> filenames_to_log;
  for (const auto [i, filename] : std::views::enumerate(methylome_names))
    filenames_to_log.emplace_back(std::format("Methylome{}", i), filename);
  log_args<log_level_t::debug>(filenames_to_log);

  std::error_code ec;
  double read_time{};

  // ADS: first read the last methylome in the input files list as we only
  // do n-1 merges so we need one to start with; we can't merge an
  // empty methylome, so we need a real one outside the loop
  const auto &last_methylome = methylome_names.back();
  auto read_start = std::chrono::high_resolution_clock::now();
  auto meth = methylome::read(methylome_directory, last_methylome, ec);
  auto read_stop = std::chrono::high_resolution_clock::now();
  read_time += duration(read_start, read_stop);
  if (ec) {
    lgr.error("Error reading methylome {} {}: {}", methylome_directory,
              last_methylome, ec);
    return EXIT_FAILURE;
  }

  double merge_time{};

  // ADS: consider threads here; no reason not to

  // ADS: iterate through the first n-1 methylomes, merge each with the n-th
  for (const auto &name :
       methylome_names | std::views::take(n_methylomes - 1)) {
    read_start = std::chrono::high_resolution_clock::now();
    const auto tmp_meth = methylome::read(methylome_directory, name, ec);
    read_stop = std::chrono::high_resolution_clock::now();
    read_time += duration(read_start, read_stop);
    if (ec) {
      lgr.error("Error reading methylome {}: {}", methylome_directory, name,
                ec);
      return EXIT_FAILURE;
    }
    const auto is_consistent = meth.is_consistent(tmp_meth);
    if (!is_consistent) {
      lgr.error("Inconsistent metadata: {} {}", last_methylome, name);
      return EXIT_FAILURE;
    }
    const auto merge_start = std::chrono::high_resolution_clock::now();
    meth.add(tmp_meth);
    const auto merge_stop = std::chrono::high_resolution_clock::now();
    merge_time += duration(merge_start, merge_stop);
  }

  ec = meth.update_metadata();
  if (ec) {
    lgr.error("Error updating metadata: {}", ec);
    return EXIT_FAILURE;
  }

  const auto write_start = std::chrono::high_resolution_clock::now();
  meth.write(methylome_outdir, merged_name, ec);
  const auto write_stop = std::chrono::high_resolution_clock::now();
  if (ec) {
    lgr.error("Error writing methylome {} {}: {}", methylome_outdir,
              merged_name, ec);
    return EXIT_FAILURE;
  }
  const auto write_time = duration(write_start, write_stop);

  std::vector<std::tuple<std::string, std::string>> timing_to_log{
    // clang-format off
    {"read time", std::format("{:.3}s", read_time)},
    {"merge time", std::format("{:.3}s", merge_time)},
    {"write time", std::format("{:.3}s", write_time)},
    // clang-format on
  };
  log_args<log_level_t::debug>(timing_to_log);

  return EXIT_SUCCESS;
}
