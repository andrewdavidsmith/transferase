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

/* mc16: work with the mc16 methylome format
 */

#include "bins.hpp"
#include "check.hpp"
#include "compress.hpp"
#include "index.hpp"
#include "lookup.hpp"
#include "merge.hpp"
#include "server_interface.hpp"
#include "zip.hpp"

#include <config.h>

#include <boost/program_options.hpp>

#include <algorithm>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using std::cend;
using std::cerr;
using std::format;
using std::println;
using std::string;
using std::string_view;
using std::vector;

namespace rg = std::ranges;
namespace vs = std::views;

namespace po = boost::program_options;
using po::value;

typedef std::function<int(int, char **)> main_fun;
const std::tuple<string_view, main_fun, string_view> commands[] = {
  // clang-format off
  {"index", index_main, "make an index for a reference genome"},
  {"compress", compress_main, "convert a methylome from xsym to mc16"},
  {"check", check_main, "check that an mc16 file is valid for a reference"},
  {"lookup", lookup_main, "get methylation levels in each interval"},
  {"merge", merge_main, "merge a set of mc16 format methylomes"},
  {"zip", zip_main, "make an mc16 format methylome smaller"},
  {"bins", bins_main, "get methylation levels in each bin"},
  {"server", server_interface_main, "run a server to respond to lookup queries"},
  // clang-format on
};

static auto
format_help(const string &description, rg::input_range auto &&cmds) {
  static constexpr auto sep_width = 4;
  const auto cmds_comma = cmds | vs::elements<0> | vs::join_with(',');
  const auto cmds_line = string(cbegin(cmds_comma), cend(cmds_comma));
  println("usage: {} {{{}}}\n\n"
          "version: {}\n",
          description, cmds_line, VERSION);
  println("commands:\n"
          "  {{{}}}",
          cmds_line);
  const auto sz =
    rg::max(vs::transform(cmds | vs::elements<0>, &string_view::size));
  rg::for_each(cmds, [sz, w = sep_width](const auto &cmd) {
    println("    {:{}}{}", get<0>(cmd), sz + w, get<2>(cmd));
  });
}

int
main(int argc, char *argv[]) {
  static constexpr auto program = "mc16";

  string command;
  po::options_description arguments;
  arguments.add_options()
    // clang-format off
    ("command", value(&command))
    ("subargs", value<vector<string>>())
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

  const auto cmd_itr = rg::find_if(
    commands, [&command](const auto c) { return get<0>(c) == command; });

  if (cmd_itr == cend(commands)) {
    format_help(program, commands);
    return EXIT_FAILURE;
  }

  return get<1>(*cmd_itr)(argc - 1, argv + 1);
}
