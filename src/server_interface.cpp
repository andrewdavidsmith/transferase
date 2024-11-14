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

#include "server_interface.hpp"

#include "logger.hpp"
#include "methylome_set.hpp"
#include "server.hpp"

#include <boost/program_options.hpp>

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <print>
#include <string>
#include <tuple>
#include <vector>

using std::cerr;
using std::cout;
using std::format;
using std::make_shared;
using std::ofstream;
using std::ostream;
using std::print;
using std::println;
using std::shared_ptr;
using std::string;
using std::tuple;
using std::uint32_t;
using std::vector;

namespace po = boost::program_options;
namespace rg = std::ranges;
namespace fs = std::filesystem;

static auto
get_canonical_dir(const string &methylome_dir) -> string {
  std::error_code ec;
  const string canonical_dir = fs::canonical(methylome_dir, ec);
  if (ec) {
    logger::instance().error("Error: {} ({})", ec, methylome_dir);
    return {};
  }
  const bool isdir = fs::is_directory(canonical_dir, ec);
  if (ec) {
    logger::instance().error("Error: {} ({})", ec, canonical_dir);
    return {};
  }
  if (!isdir) {
    logger::instance().error("Not a directory: {}", canonical_dir);
    return {};
  }
  return canonical_dir;
}

template <mxe_log_level the_level, typename... Args>
auto
log_args(rg::input_range auto &&key_value_pairs) {
  logger &lgr = logger::instance();
  for (auto &&[k, v] : key_value_pairs)
    lgr.log<the_level>("{}: {}", k, v);
}

struct server_interface_args {
  static constexpr mxe_log_level default_log_level{mxe_log_level::debug};
  static constexpr auto default_n_threads{4};
  std::uint32_t n_threads{};
  mxe_log_level log_level{};
  std::string port{"5000"};
  std::string hostname{};
  std::string methylome_dir{};
  std::string log_filename{};
  std::uint32_t max_live_methylomes{};
  bool daemonize{};
};

auto
server_interface_main(int argc, char *argv[]) -> int {
  static const auto description = "server";

  server_interface_args args{};

  po::options_description desc(description);
  desc.add_options()
    // clang-format off
    ("help,h", "produce help message")
    ("hostname,H", po::value(&args.hostname)->required(), "server hostname")
    ("port,p", po::value(&args.port)->required(), "port")
    ("methylomes,m", po::value(&args.methylome_dir)->required(), "methylome dir")
    ("threads,t", po::value(&args.n_threads)->default_value(args.default_n_threads),
     "number of threads")
    ("live,l", po::value(&args.max_live_methylomes)->default_value(
     methylome_set::default_max_live_methylomes), "max live methylomes")
    ("log", po::value(&args.log_filename), "log file name")
    ("log-level,v", po::value(&args.log_level)->default_value(
     server_interface_args::default_log_level), "log file name")
    ("daemonize", po::bool_switch(&args.daemonize), "daemonize the server")
    // clang-format on
    ;
  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help")) {
      desc.print(cout);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    println(cerr, "{}", e.what());
    desc.print(cerr);
    return EXIT_FAILURE;
  }

  shared_ptr<ostream> log_file =
    args.log_filename.empty()
      ? make_shared<ostream>(cout.rdbuf())
      : make_shared<ofstream>(args.log_filename, std::ios::app);

  logger &lgr = logger::instance(log_file, description, args.log_level);
  if (!lgr) {
    println("Failure initializing logging: {}.", lgr.get_status().message());
    return EXIT_FAILURE;
  }

  args.methylome_dir = get_canonical_dir(args.methylome_dir);
  // messages done already
  if (args.methylome_dir.empty())
    return EXIT_FAILURE;

  log_args<mxe_log_level::info>(vector<tuple<string, string>>{
    // clang-format off
    {"Hostname", args.hostname},
    {"Port", args.port},
    {"Log file", args.log_filename.empty() ? "console" : args.log_filename},
    {"Methylome directory", args.methylome_dir},
    {"Max live methylomes", format("{}", args.max_live_methylomes)},
    // clang-format on
  });

  if (args.daemonize) {
    std::error_code ec;
    server s(args.hostname, args.port, args.n_threads, args.methylome_dir,
             args.max_live_methylomes, lgr, ec);
    if (ec) {
      lgr.error("Failure daemonizing server: {}.", ec);
      return EXIT_FAILURE;
    }
    s.run();
  }
  else {
    server s(args.hostname, args.port, args.n_threads, args.methylome_dir,
             args.max_live_methylomes, lgr);

    s.run();
  }

  return EXIT_SUCCESS;
}
