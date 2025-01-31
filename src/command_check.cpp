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

static constexpr auto about = R"(
check the given files for correctness and consistency
)";

static constexpr auto description = R"(
Perform 3 kinds of checks. First, the index is checked internally to
verify that the index data and the index metadata are consistent.
Second, the methylomes are each checked internally to verify that the
methylome data and methylome metadata is consistent for each given
methylome. Finally, each given methylome is checked for consistency
with the given index. Not output is written except that logged to the
console. The exit code of the app will be non-zero if any of the
consistency checks fails. At a log-level of 'debug' the outcome of
each check will be logged so the cause of any failure can be
determined.
)";

static constexpr auto examples = R"(
Examples:

xfr check -x index_dir -d methylome_dir -g hg38 -m SRX012345 SRX612345
)";

#include "genome_index.hpp"
#include "logger.hpp"
#include "metadata_is_consistent.hpp"
#include "methylome.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <format>
#include <iostream>
#include <iterator>  // for std::cbegin, std::cend
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>  // IWYU pragma: keep
#include <vector>

auto
command_check_main(int argc,
                   char *argv[])  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  -> int {
  static constexpr auto command = "check";
  static const auto usage =
    std::format("Usage: xfr {} [options]\n", rstrip(command));
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  std::string index_directory{};
  std::string genome_name{};
  std::string methylome_directory{};
  transferase::log_level_t log_level{};

  namespace po = boost::program_options;

  po::options_description desc("Options");
  desc.add_options()
    // clang-format off
    ("help,h", "print this message and exit")
    ("index-dir,x", po::value(&index_directory)->required(), "genome index directory")
    ("genome,g", po::value(&genome_name)->required(), "genome name")
    ("methylome-dir,d", po::value(&methylome_directory)->required(),
     "directory containing methylomes")
    ("methylomes,m", po::value<std::vector<std::string>>()->multitoken()->required(),
     "methylome names")
    ("log-level,v", po::value(&log_level)->default_value(transferase::logger::default_level),
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

  auto &lgr = transferase::logger::instance(transferase::shared_from_cout(),
                                            command, log_level);
  if (!lgr) {
    const auto status = lgr.get_status();
    std::println("Failure initializing logging: {}.", status.message());
    return EXIT_FAILURE;
  }

  const auto methylomes = vm["methylomes"].as<std::vector<std::string>>();

  const auto joined = methylomes | std::views::join_with(',');
  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Index directory", index_directory},
    {"Genome", genome_name},
    {"Methylome directory", methylome_directory},
    {"Methylomes", std::string(std::cbegin(joined), std::cend(joined))},
    // clang-format on
  };
  transferase::log_args<transferase::log_level_t::info>(args_to_log);

  std::error_code index_read_err;
  const auto index = transferase::genome_index::read(
    index_directory, genome_name, index_read_err);
  if (index_read_err) {
    lgr.error("Failed to read genome index {} {}: {}", index_directory,
              genome_name, index_read_err);
    return EXIT_FAILURE;
  }
  const auto genome_index_consistency = index.is_consistent();
  lgr.info("Index data and metadata consistent: {}", genome_index_consistency);

  bool all_methylomes_consitent = true;
  bool all_methylomes_metadata_consitent = true;
  for (const auto &methylome_name : methylomes) {
    std::error_code ec;
    const auto meth =
      transferase::methylome::read(methylome_directory, methylome_name, ec);
    if (ec) {
      lgr.error("Failed to read methylome {}: {}", methylome_name, ec);
      return EXIT_FAILURE;
    }

    lgr.info("Methylome methylation levels: {}", meth.global_levels_covered());

    const auto methylome_consistency = meth.is_consistent();
    lgr.info("Methylome data and metadata are consistent: {}",
             methylome_consistency);
    all_methylomes_consitent =
      all_methylomes_consitent && methylome_consistency;

    const auto metadata_consistency = metadata_is_consistent(meth, index);
    lgr.info("Methylome and index metadata consistent: {}",
             metadata_consistency);
    all_methylomes_metadata_consitent =
      all_methylomes_metadata_consitent && metadata_consistency;
  }

  lgr.info("all methylomes consistent: {}", all_methylomes_consitent);
  lgr.info("all methylome metadata consistent: {}",
           all_methylomes_metadata_consitent);

  const auto ret_val = genome_index_consistency && all_methylomes_consitent &&
                       all_methylomes_metadata_consitent;

  return ret_val ? EXIT_SUCCESS : EXIT_FAILURE;
}
