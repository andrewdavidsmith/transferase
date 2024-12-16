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

/* xfrase: methylome transfer engine
 */

#include "command_bins.hpp"
#include "command_check.hpp"
#include "command_compress.hpp"
#include "command_config.hpp"
#include "command_format.hpp"
#include "command_index.hpp"
#include "command_intervals.hpp"
#include "command_merge.hpp"
#include "command_server.hpp"

#include <config.h>  // for VERSION

#include <boost/program_options.hpp>

#include <algorithm>
#include <cstdlib>  // for EXIT_FAILURE
#include <format>
#include <fstream>
#include <functional>  // for function
#include <iterator>    // for cend, cbegin
#include <print>
#include <ranges>
#include <string>
#include <string_view>  // for string_view
#include <tuple>
#include <vector>

using std::cbegin;
using std::cend;
using std::format;
using std::string;
using std::string_view;
using std::vector;

namespace rg = std::ranges;
namespace vs = std::views;

namespace po = boost::program_options;
using po::value;

typedef std::function<int(int, char **)> main_fun;
const std::tuple<std::string_view, main_fun, std::string_view> commands[] = {
  // clang-format off
  {"config", command_config_main, "configure a client for remote queries"},
  {"index", command_index_main, "make an index for a reference genome"},
  {"format", command_format_main, "format a methylome file"},
  {"check", command_check_main, "perform checks on methylome and index files"},
  {"intervals", command_intervals_main, "get methylation levels in each interval"},
  {"merge", command_merge_main, "merge a set of xfrase format methylomes"},
  {"compress", command_compress_main, "make an xfrase format methylome smaller"},
  {"bins", command_bins_main, "get methylation levels in each bin"},
  {"server", command_server_main, "run a server to respond to lookup queries"},
  // clang-format on
};

static auto
format_help(const std::string &description,
            std::ranges::input_range auto &&cmds) {
  static constexpr auto sep_width = 4;
  const auto cmds_comma =
    cmds | std::views::elements<0> | std::views::join_with(',');
  const std::string cmds_line(std::cbegin(cmds_comma), std::cend(cmds_comma));
  std::println("usage: {} {{{}}}\n\n"
               "version: {}\n",
               description, cmds_line, VERSION);
  std::println("commands:\n"
               "  {{{}}}",
               cmds_line);
  const auto sz = std::ranges::max(std::views::transform(
    cmds | std::views::elements<0>, &std::string_view::size));
  std::ranges::for_each(cmds, [sz, w = sep_width](const auto &cmd) {
    std::println("    {:{}}{}", get<0>(cmd), sz + w, get<2>(cmd));
  });
}

int
main(int argc, char *argv[]) {
  static constexpr auto program = "xfrase";

  std::string command;
  po::options_description arguments;
  arguments.add_options()
    // clang-format off
    ("command", value(&command))
    ("subargs", value<vector<std::string>>())
    // clang-format on
    ;
  // positional; one for "command" and the rest in "subargs"
  po::positional_options_description p;
  p.add("command", 1).add("subargs", -1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv)
              .options(arguments)
              .positional(p)
              .allow_unregistered()
              .run(),
            vm);
  if (!vm.count("command")) {
    format_help(program, commands);
    return EXIT_SUCCESS;
  }
  po::notify(vm);

  const auto cmd_itr = std::ranges::find_if(
    commands, [&command](const auto c) { return get<0>(c) == command; });

  if (cmd_itr == cend(commands)) {
    format_help(program, commands);
    return EXIT_FAILURE;
  }

  return get<1>(*cmd_itr)(argc - 1, argv + 1);
}
