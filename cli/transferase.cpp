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

/* transferase: methylome transfer engine
 */

#include "command_check.hpp"
#include "command_compress.hpp"
#include "command_config.hpp"
#include "command_format.hpp"
#include "command_index.hpp"
#include "command_list.hpp"
#include "command_merge.hpp"
#include "command_query.hpp"
#include "command_select.hpp"
#include "command_server.hpp"
#include "command_server_config.hpp"

#include <config.h>  // for VERSION

#include "CLI11/CLI11.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>     // for EXIT_FAILURE
#include <exception>   // for std::set_terminate
#include <functional>  // for function
#include <iostream>
#include <iterator>  // for cend, cbegin
#include <print>
#include <ranges>
#include <string>
#include <string_view>  // for string_view
#include <tuple>
#include <variant>  // for std::tuple
#include <vector>

typedef std::function<int(int, char **)> main_fun;
typedef std::tuple<std::string_view, main_fun, std::string_view> cmd;
const auto commands = std::array{
  // clang-format off
  cmd{"config", command_config_main, "configure a client for remote queries"},
  cmd{"server-config", command_server_config_main, "generate a server config file"},
  cmd{"list", command_list_main, "list methylome or indexs in a directory"},
#ifdef HAVE_NCURSES
  cmd{"select", command_select_main, "select methylomes from those available"},
#endif
  cmd{"index", command_index_main, "make an index for a reference genome"},
  cmd{"format", command_format_main, "format a methylome file"},
  cmd{"check", command_check_main, "perform checks on methylome and index files"},
  cmd{"merge", command_merge_main, "merge a set of transferase format methylomes"},
  cmd{"compress", command_compress_main, "make an transferase format methylome smaller"},
  cmd{"query", command_query_main, "query methylation levels"},
  cmd{"server", command_server_main, "run a server to respond to lookup queries"},
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
main(int argc, char *argv[]) {  // NOLINT(*-c-arrays)
  static constexpr auto program = "transferase";

  std::set_terminate([]() {
    std::println(std::cerr, "Terminating due to critical error");
    std::abort();
  });

  if (argc <= 1) {
    format_help(program, commands);
    return EXIT_SUCCESS;
  }

  std::string command = argv[1];

  const auto cmd_itr = std::ranges::find_if(
    commands, [&command](const auto &c) { return get<0>(c) == command; });

  if (cmd_itr == std::cend(commands)) {
    format_help(program, commands);
    return EXIT_FAILURE;
  }

  return get<1>(*cmd_itr)(argc - 1, argv + 1);
}
