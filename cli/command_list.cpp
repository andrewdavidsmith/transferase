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

#include "command_list.hpp"

static constexpr auto about = R"(
list methylomes or index files in a given directory
)";

static constexpr auto description = R"(
List either the methylomes or index files in a given directory based
on their filenames and filename extenionsions.
)";

static constexpr auto examples = R"(
Examples:

xfr list /path/to/some_directory ../relative/path
)";

#include "cli_common.hpp"
#include "format_error_code.hpp"  // IWYU pragma: keep
#include "genome_index.hpp"
#include "methylome.hpp"
#include "utilities.hpp"

#include "CLI11/CLI11.hpp"

#include <algorithm>
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <print>
#include <ranges>  // IWYU pragma: keep
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

auto
command_list_main(int argc, char *argv[])  // NOLINT(*-c-arrays)
  -> int {
  static constexpr auto command = "list";
  static const auto usage =
    std::format("Usage: xfr {} [options]", rstrip(command));
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  bool verbose{false};
  bool show_only_indexes{true};
  bool show_only_methylomes{true};
  std::vector<std::string> directories;

  CLI::App app{about_msg};
  argv = app.ensure_utf8(argv);
  app.usage(usage);
  if (argc >= 2)
    app.footer(description_msg);
  app.get_formatter()->column_width(column_width_default);
  app.get_formatter()->label("REQUIRED", "REQD");
  app.set_help_flag("-h,--help", "Print a detailed help message and exit");
  // clang-format off
  const auto indexes_only_opt =
    app.add_flag("-x,--indexes-only", show_only_indexes,
                 "show only cpg indexes")
    ->option_text(" ");
  app.add_flag("-m,--methylomes-only", show_only_methylomes,
               "show only methylomes")
    ->option_text(" ")
    ->excludes(indexes_only_opt);
  app.add_option("-d,--directories", directories,
                 "search these directories")
    ->required();
  app.add_flag("-v,--verbose", verbose, "print more info");
  // clang-format on

  if (argc < 2) {
    std::println("{}", app.help());
    return EXIT_SUCCESS;
  }

  CLI11_PARSE(app, argc, argv);

  std::error_code ec;
  std::vector<std::string> canonical_directories;
  for (auto const &given_dirname : directories) {
    const auto canonical = std::filesystem::canonical(given_dirname, ec);
    if (ec) {
      std::println("Error {}: {}", given_dirname, ec.message());
      return EXIT_FAILURE;
    }
    canonical_directories.emplace_back(canonical);
  }

  const auto print = [](auto &&name) { std::println("{}", name); };

  for (auto const &dirname : canonical_directories) {
    if (verbose)
      std::println("directory: {}", dirname);
    if (!show_only_methylomes) {
      if (verbose && !show_only_indexes)
        std::println("indexes:");
      const auto index_names = transferase::genome_index::list(dirname, ec);
      if (ec) {
        std::println("Error {}: {}", dirname, ec);
        return EXIT_FAILURE;
      }
      std::ranges::for_each(index_names, print);
      if (verbose && !show_only_indexes)
        std::println();
    }
    if (!show_only_indexes) {
      if (verbose && !show_only_methylomes)
        std::println("methylomes:");
      const auto methylome_names = transferase::methylome::list(dirname, ec);
      if (ec) {
        std::println("Error {}: {}", dirname, ec);
        return EXIT_FAILURE;
      }
      std::ranges::for_each(methylome_names, print);
      if (verbose && !show_only_methylomes)
        std::println();
    }
  }

  return EXIT_SUCCESS;
}
