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

xfr index -v debug -x /path/to/index_directory -g hg38.fa
)";

#include "cli_common.hpp"
#include "format_error_code.hpp"  // IWYU pragma: keep
#include "genome_index.hpp"
#include "logger.hpp"
#include "utilities.hpp"

#include "CLI11/CLI11.hpp"

#include <chrono>
#include <format>
#include <map>
#include <memory>
#include <print>
#include <stdlib.h>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>  // IWYU pragma: keep
#include <vector>

auto
command_index_main(int argc, char *argv[]) -> int {  // NOLINT(*-c-arrays)
  static constexpr auto log_level_default = transferase::log_level_t::info;
  static constexpr auto command = "index";
  static const auto usage =
    std::format("Usage: xfr {} [options]", rstrip(command));
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  namespace xfr = transferase;

  std::string genome_filename{};
  std::string index_file{};
  std::string index_directory{};
  xfr::log_level_t log_level{log_level_default};

  CLI::App app{about_msg};
  argv = app.ensure_utf8(argv);
  app.usage(usage);
  if (argc >= 2)
    app.footer(description_msg);
  app.get_formatter()->column_width(column_width_default);
  app.get_formatter()->label("REQUIRED", "REQD");
  app.set_help_flag("-h,--help", "Print a detailed help message and exit");
  // clang-format off
  app.add_option("-g,--genome", genome_filename,
                 "genome_file")->required();
  app.add_option("-x,--index-dir", index_directory,
                 "index output directory")->required();
  app.add_option("-v,--log-level", log_level,
                 std::format("log level {}", xfr::log_level_help_str))
    ->option_text(std::format("[{}]", log_level_default))
    ->transform(CLI::CheckedTransformer(xfr::str_to_level, CLI::ignore_case));
  // clang-format on

  if (argc < 2) {
    std::println("{}", app.help());
    return EXIT_SUCCESS;
  }
  CLI11_PARSE(app, argc, argv);

  auto &lgr =
    xfr::logger::instance(xfr::shared_from_cout(), command, log_level);
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
  xfr::log_args<xfr::log_level_t::info>(args_to_log);

  std::error_code error;
  const auto genome_name =
    xfr::genome_index::parse_genome_name(genome_filename, error);
  if (error) {
    lgr.error("Failed to parse genome name from: {}", genome_filename);
    return EXIT_FAILURE;
  }
  lgr.info("Identified genome name: {}", genome_name);

  const auto constr_start = std::chrono::high_resolution_clock::now();
  const auto index =
    xfr::genome_index::make_genome_index(genome_filename, error);
  const auto constr_stop = std::chrono::high_resolution_clock::now();
  if (error) {
    if (error == std::errc::no_such_file_or_directory)
      lgr.error("Genome file not found: {}", genome_filename);
    else
      lgr.error("Error constructing index: {}", error);
    return EXIT_FAILURE;
  }
  lgr.debug("Index construction time: {:.3}s",
            duration(constr_start, constr_stop));

  // Check on the output directory; if it doesn't exist, make it
  validate_output_directory(index_directory, error);
  if (error) {
    lgr.error("Terminating due to error");
    return EXIT_FAILURE;
  }

  index.write(index_directory, genome_name, error);
  if (error) {
    lgr.error("Error writing cpg index {} {}: {}", index_directory, genome_name,
              error);
    return EXIT_FAILURE;
  }
  lgr.info("Completed index construction");

  return EXIT_SUCCESS;
}
