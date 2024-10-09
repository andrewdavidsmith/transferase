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

#include "lookup_server.hpp"

#include "methylome_set.hpp"
#include "server.hpp"

#include <boost/program_options.hpp>

#include <cstdint>
#include <iostream>
#include <print>
#include <string>

using std::cerr;
using std::print;
using std::println;
using std::string;
using std::uint32_t;

namespace po = boost::program_options;

auto
lookup_server_main(int argc, char *argv[]) -> int {
  static constexpr auto default_n_threads{4};

  static const auto description = "server";

  bool verbose{};

  uint32_t n_threads{};
  string port{};
  string hostname{};
  string methylome_dir;
  uint32_t max_live_methylomes{};

  po::options_description desc(description);
  // clang-format off
  desc.add_options()
    ("help,h", "produce help message")
    ("port,p", po::value(&port)->required(), "port")
    ("hostname,H", po::value(&hostname)->required(), "server hostname")
    ("methylomes,m", po::value(&methylome_dir)->required(), "methylome dir")
    ("threads,t", po::value(&n_threads)->default_value(default_n_threads),
     "number of threads")
    ("live,l", po::value(&max_live_methylomes)->default_value(
     methylome_set::default_max_live_methylomes), "max live methylomes")
    ("verbose,v", po::bool_switch(&verbose), "print more run info")
    ;
  // clang-format on
  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help")) {
      desc.print(std::cout);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    println(cerr, "{}", e.what());
    desc.print(cerr);
    return EXIT_FAILURE;
  }

  if (verbose)
    println("Hostname: {}\n"
            "Port: {}\n"
            "Methylome directory: {}\n"
            "Max live methylomes: {}\n",
            hostname, port, methylome_dir, max_live_methylomes);

  server s(hostname, port, n_threads, methylome_dir, max_live_methylomes,
           verbose);

  s.run();

  return EXIT_SUCCESS;
}
