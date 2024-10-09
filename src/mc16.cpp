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

#include "lookup_client.hpp"
#include "lookup_server.hpp"

#include "lookup_client_sync.hpp"
#include "lookup_server_sync.hpp"

#include <config.h>

#include <boost/program_options.hpp>

#include <algorithm>
#include <cassert>
#include <format>
#include <fstream>
#include <iostream>
#include <numeric>
#include <print>
#include <ranges>
#include <string>
#include <vector>

using std::cend;
using std::cerr;
using std::format;
using std::formatter;
using std::pair;
using std::println;
using std::string;
using std::string_view;
using std::vector;

namespace rg = std::ranges;
namespace vs = std::views;

namespace po = boost::program_options;
using po::value;

template <typename T> using num_lim = std::numeric_limits<T>;

struct mc16_command {
  string tag;
  string description;
  std::function<int(const int, const char **)> fun;

  auto operator()(const int argc, const char **argv) const -> int {
    return fun(argc - 1, argv + 1);
  }
};

template <> struct std::formatter<mc16_command> : formatter<string> {
  auto format(const mc16_command &cmd, format_context &ctx) const {
    static constexpr auto pad_size = 4;
    static constexpr auto pad = string(pad_size, ' ');
    return std::formatter<std::string>::format(
      std::format("{}: {}", cmd.tag, cmd.description), ctx);
  }
};

typedef std::function<int(int, char **)> main_fun;

// clang-format off
const pair<string_view, main_fun> commands[] = {
  {"index", index_main},
  {"compress", compress_main},
  {"check", check_main},
  {"lookup", lookup_main},
  {"merge", merge_main},
  {"bins", bins_main},
  {"client", lookup_client_main},
  {"server", lookup_server_main},
  {"lookup-client", lookup_client_sync_main},
  {"lookup-server", lookup_server_sync_main},
};
// clang-format on

constexpr string_view command_names[] = {
  "index", "compress", "check",  "merge",       "lookup",
  "bins",  "client",   "server", "client-sync", "server-sync",
};

static auto
format_help(const string &description, rg::input_range auto &&command_names) {
  print("Usage: {}\n"
        "COMMAND may be one of the following:\n",
        description);
  for (const auto &cmd_nm : command_names)
    print("\t{}\n", cmd_nm);
}

int
main(int argc, char *argv[]) {
  static constexpr auto program = "mc16";
  const auto description = format("{} COMMAND <command_arguments>", program);
  const auto cmd_nm = command_names | vs::join_with(' ');
  const auto cmd_descr = "{ " + string(cbegin(cmd_nm), cend(cmd_nm)) + " }";

  string command;
  po::options_description arguments;
  arguments.add_options()
    // clang-format off
    ("command", value(&command), cmd_descr.data())
    ("subargs", value<vector<string>>(), "the sub arguments")
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
    format_help(description, command_names);
    return EXIT_SUCCESS;
  }
  po::notify(vm);

  const auto cmd_itr = rg::find_if(
    commands, [&command](const auto c) { return c.first == command; });

  if (cmd_itr == cend(commands)) {
    format_help(description, command_names);
    return EXIT_FAILURE;
  }

  return cmd_itr->second(argc - 1, argv + 1);
}
