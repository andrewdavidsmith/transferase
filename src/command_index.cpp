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

#include "command_index.hpp"

static constexpr auto about = R"(
make an index for a given reference genome
)";

static constexpr auto description = R"(
The genome index is used to accelerate searches within methylomes and
must be created from the same reference genome that was used
originally to map the reads and generate the single-CpG methylation
levels. The order of chromosomes within the reference genome is not
relevant as long as each chromosome is correct. The index is in two
files, one a binary file (size just over 100MB for hg38), and the
other a metadata file in JSON format file that can be examined with
any JSON formatter (e.g., jq or json_pp).  These two files must reside
together in the same directory.
)";

static constexpr auto examples = R"(
Examples:

xfrase index -v debug -x /path/to/index_directory -g hg38.fa
)";

#include "format_error_code.hpp"  // IWYU pragma: keep
#include "genome_index.hpp"
#include "logger.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <chrono>
#include <format>
#include <iostream>
#include <print>
#include <stdlib.h>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>  // IWYU pragma: keep
#include <vector>

auto
command_index_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "index";
  static const auto usage =
    std::format("Usage: xfrase {} [options]\n", rstrip(command));
  static const auto about_msg =
    std::format("xfrase {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  std::string genome_filename{};
  std::string index_file{};
  std::string index_directory{};
  transferase::log_level_t log_level{};

  namespace po = boost::program_options;

  po::options_description desc("Options");
  desc.add_options()
    // clang-format off
    ("help,h", "print this message and exit")
    ("genome,g", po::value(&genome_filename)->required(), "genome_file")
    ("indexdir,x", po::value(&index_directory)->required(), "index output directory")
    ("log-level,v", po::value(&log_level)->default_value(transferase::logger::default_level),
     "log level {debug,info,warning,error,critical}")
    // clang-format on
    ;
  try {
    po::variables_map vm;
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
    lgr.error("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Genome", genome_filename},
    {"Index directory", index_directory},
    // clang-format on
  };
  transferase::log_args<transferase::log_level_t::info>(args_to_log);

  std::error_code ec;
  const auto genome_name =
    transferase::genome_index::parse_genome_name(genome_filename, ec);
  if (ec) {
    lgr.error("Failed to parse genome name from: {}", genome_filename);
    return EXIT_FAILURE;
  }
  lgr.info("Identified genome name: {}", genome_name);

  const auto constr_start = std::chrono::high_resolution_clock::now();
  const auto index =
    transferase::genome_index::make_genome_index(genome_filename, ec);
  const auto constr_stop = std::chrono::high_resolution_clock::now();
  if (ec) {
    if (ec == std::errc::no_such_file_or_directory)
      lgr.error("Genome file not found: {}", genome_filename);
    else
      lgr.error("Error constructing index: {}", ec);
    return EXIT_FAILURE;
  }
  lgr.debug("Index construction time: {:.3}s",
            duration(constr_start, constr_stop));

  ec = index.write(index_directory, genome_name);
  if (ec) {
    lgr.error("Error writing cpg index {} {}: {}", index_directory, genome_name,
              ec);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
