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

xfrase list /path/to/some_directory ../relative/path
)";

#include "cpg_index.hpp"
#include "methylome.hpp"
#include "utilities.hpp"
#include "xfrase_error.hpp"  // IWYU pragma: keep

#include <boost/program_options.hpp>

#include <algorithm>
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <filesystem>
#include <format>
#include <iostream>
#include <print>
#include <ranges>  // IWYU pragma: keep
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

auto
command_list_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "list";
  static const auto usage =
    std::format("Usage: xfrase {} [options]\n", strip(command));
  static const auto about_msg =
    std::format("xfrase {}: {}", strip(command), strip(about));
  static const auto description_msg =
    std::format("{}\n{}", strip(description), strip(examples));

  bool verbose{false};

  namespace po = boost::program_options;
  bool show_only_indexes{true};
  bool show_only_methylomes{true};
  std::vector<std::string> directories;

  po::options_description desc("Options");
  desc.add_options()
    // clang-format off
    ("help,h", "print this message and exit")
    ("indexes-only,x", po::bool_switch(&show_only_indexes), "show only cpg indexes")
    ("methylomes-only,m", po::bool_switch(&show_only_methylomes), "show only methylomes")
    ("directories,d", po::value<std::vector<std::string>>()->required(),
     "search these directories (flag is optional)")
    ("verbose,v", po::bool_switch(&verbose), "print more info")
    // clang-format on
    ;
  po::positional_options_description p;
  p.add("directories", -1);
  po::variables_map vm;
  try {
    po::store(
      po::command_line_parser(argc, argv).options(desc).positional(p).run(),
      vm);
    if (vm.count("help") || argc == 1) {
      std::println("{}\n{}", about_msg, usage);
      desc.print(std::cout);
      std::println("\n{}", description_msg);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
    directories = vm["directories"].as<std::vector<std::string>>();
  }
  catch (po::error &e) {
    std::println("{}", e.what());
    std::println("{}\n{}", about_msg, usage);
    desc.print(std::cout);
    std::println("\n{}", description_msg);
    return EXIT_FAILURE;
  }

  std::error_code ec;
  std::vector<std::string> canonical_directories;
  for (auto const &given_dirname : directories) {
    const auto canonical = std::filesystem::canonical(given_dirname, ec);
    if (ec) {
      std::println("Error {}: {}", given_dirname, ec);
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
      const auto index_names = xfrase::cpg_index::list_cpg_indexes(dirname, ec);
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
      const auto methylome_names = xfrase::methylome::list(dirname, ec);
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
