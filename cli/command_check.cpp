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
Perform 3 kinds of checks. First, the index is checked internally to verify
that the index data and the index metadata are consistent.  Second, the
methylomes are each checked internally to verify that the methylome data and
methylome metadata is consistent for each given methylome. Finally, each given
methylome is checked for consistency with the given index. Not output is
written except that logged to the console. The exit code of the app will be
non-zero if any of the consistency checks fails. At a log-level of 'debug' the
outcome of each check will be logged so the cause of any failure can be
determined.
)";

static constexpr auto examples = R"(
Examples:

xfr check -x index_dir -d methylome_dir
)";

#include "cli_common.hpp"
#include "genome_index.hpp"
#include "genome_index_set.hpp"
#include "logger.hpp"
#include "metadata_is_consistent.hpp"
#include "methylome.hpp"
#include "methylome_set.hpp"
#include "utilities.hpp"

#include "CLI11/CLI11.hpp"

#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <format>
#include <iterator>  // for std::cbegin, std::cend
#include <map>
#include <memory>  // for std::shared_ptr
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>  // IWYU pragma: keep
#include <vector>

auto
command_check_main(int argc, char *argv[]) -> int {  // NOLINT(*-c-arrays)
  static constexpr auto log_level_default = transferase::log_level_t::info;
  static constexpr auto command = "check";
  static const auto usage =
    std::format("Usage: xfr {} [options]", rstrip(command));
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  namespace xfr = transferase;

  std::string index_dir{};
  std::string genome_name_arg{};
  std::vector<std::string> methylome_names;
  std::string methylome_dir{};
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
  app.add_option("-x,--index-dir", index_dir,
                 "genome index directory")
    ->required()
    ->check(CLI::ExistingDirectory);
  app.add_option("-g,--genome", genome_name_arg,
                 "genome name (default: all in directory)");
  app.add_option("-d,--methylome-dir", methylome_dir,
                 "directory containing methylomes")
    ->required()
    ->check(CLI::ExistingDirectory);
  app.add_option("-m,--methylomes", methylome_names,
                 "names of methylome (default: all in directory)");
  app.add_option("-v,--log-level", log_level,
                 "{debug, info, warning, error, critical}")
    ->option_text(std::format("ENUM [{}]", log_level_default))
    ->transform(CLI::CheckedTransformer(xfr::log_level_cli11, CLI::ignore_case));
  // clang-format on

  if (argc < 2) {
    std::println("{}", app.help());
    return EXIT_SUCCESS;
  }

  CLI11_PARSE(app, argc, argv);

  auto &lgr =
    xfr::logger::instance(xfr::shared_from_cout(), command, log_level);
  if (!lgr) {
    const auto status = lgr.get_status();
    std::println("Failure initializing logging: {}.", status.message());
    return EXIT_FAILURE;
  }

  std::error_code error;

  if (methylome_names.empty()) {
    methylome_names = xfr::methylome::list(methylome_dir, error);
    if (error) {
      lgr.error("Error reading methylome directory {}: {}", methylome_dir,
                error);
      return EXIT_FAILURE;
    }
  }

  std::vector<std::string> genome_names;
  if (genome_name_arg.empty()) {
    genome_names = xfr::genome_index::list(index_dir, error);
    if (error) {
      lgr.error("Error reading genome index directory {}: {}", index_dir,
                error);
      return EXIT_FAILURE;
    }
  }
  else
    genome_names = {genome_name_arg};

  const auto joined_methylomes = methylome_names | std::views::join_with(',');
  const auto joined_genomes = genome_names | std::views::join_with(',');
  const std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Index directory", index_dir},
    {"Genomes", std::string(std::cbegin(joined_genomes),
                            std::cend(joined_genomes))},
    {"Methylome directory", methylome_dir},
    {"Methylomes", std::string(std::cbegin(joined_methylomes),
                               std::cend(joined_methylomes))},
    // clang-format on
  };
  xfr::log_args<xfr::log_level_t::info>(args_to_log);

  xfr::genome_index_set indexes(index_dir);
  if (error) {
    lgr.error("Failed to initialize genome index set: {}", error);
    return EXIT_FAILURE;
  }

  bool all_genomes_consitent = true;
  for (const auto &genome_name : genome_names) {
    const auto index = indexes.get_genome_index(genome_name, error);
    if (error) {
      lgr.error("Failed to read genome index {}: {}", genome_name, error);
      return EXIT_FAILURE;
    }
    const auto genome_index_consistency = index->is_consistent();
    lgr.info("Index data and metadata consistent for {}: {}", genome_name,
             genome_index_consistency);
    all_genomes_consitent = all_genomes_consitent && genome_index_consistency;
  }

  xfr::methylome_set methylomes(methylome_dir);
  if (error) {
    lgr.error("Failed to initialize methylome set: {}", error);
    return EXIT_FAILURE;
  }

  bool all_methylomes_consitent = true;
  bool all_methylomes_metadata_consitent = true;
  for (const auto &methylome_name : methylome_names) {
    const auto methylome = methylomes.get_methylome(methylome_name, error);
    if (error) {
      lgr.error("Failed to read methylome {}: {}", methylome_name, error);
      return EXIT_FAILURE;
    }
    const auto methylome_consistency = methylome->is_consistent();
    lgr.info("Methylome data and metadata consistent for {}: {}",
             methylome_name, methylome_consistency);
    lgr.info("Methylome methylation levels: {}",
             methylome->global_levels_covered());

    all_methylomes_consitent =
      all_methylomes_consitent && methylome_consistency;

    const auto genome_name = methylome->get_genome_name();
    const auto index = indexes.get_genome_index(genome_name, error);
    if (error) {
      lgr.error("Failed to get genome index {} required by methylome {}: {}",
                genome_name, methylome_name, error);
      return EXIT_FAILURE;
    }

    const auto metadata_consistency =
      metadata_is_consistent(*methylome, *index);
    lgr.info("Methylome and index metadata consistent: {}",
             metadata_consistency);
    all_methylomes_metadata_consitent =
      all_methylomes_metadata_consitent && metadata_consistency;
  }

  lgr.info("all methylomes consistent: {}", all_methylomes_consitent);
  lgr.info("all methylome metadata consistent: {}",
           all_methylomes_metadata_consitent);

  const auto ret_val = all_genomes_consitent && all_methylomes_consitent &&
                       all_methylomes_metadata_consitent;

  return ret_val ? EXIT_SUCCESS : EXIT_FAILURE;
}
