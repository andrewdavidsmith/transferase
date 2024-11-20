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

#include "arguments.hpp"
#include "logger.hpp"
#include "methylome_set.hpp"
#include "server.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>  // std::make_shared
#include <print>
#include <string>
#include <tuple>
#include <vector>

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

struct server_interface_argset : argset_base<server_interface_argset> {
  static constexpr auto default_config_filename = "mxe_server_config.toml";

  static auto get_default_config_file_impl() -> string {
    std::error_code ec;
    const auto config_dir = get_mxe_config_dir_default(ec);
    if (ec)
      return {};
    return fs::path(config_dir) / default_config_filename;
  }

  static constexpr auto hostname_default{"localhost"};
  static constexpr auto port_default{"5000"};
  static constexpr auto log_level_default{mxe_log_level::warning};
  static constexpr auto n_threads_default{1};
  static constexpr auto max_resident_default = 32;
  string hostname{};
  string port{};
  string methylome_dir{};
  string log_filename{};
  mxe_log_level log_level{};
  uint32_t n_threads{};
  uint32_t max_resident{};
  bool daemonize{};

  auto log_options_impl() const {
    log_args<mxe_log_level::info>(vector<tuple<string, string>>{
      // clang-format off
        {"hostname", format("{}", hostname)},
        {"port", format("{}", port)},
        {"methylome_dir", format("{}", methylome_dir)},
        {"log_filename", format("{}", log_filename)},
        {"log_level", format("{}", log_level)},
        {"n_threads", format("{}", n_threads)},
        {"max_resident", format("{}", max_resident)},
        {"daemonize", format("{}", daemonize)},
      // clang-format on
    });
  }

  [[nodiscard]] auto
  set_common_opts_impl() -> boost::program_options::options_description {
    namespace po = boost::program_options;
    using po::value;
    po::options_description opts("Command line or config file");
    opts.add_options()
      // clang-format off
      ("hostname,s", value(&hostname)->default_value(hostname_default),
       "server hostname")
      ("port,p", value(&port)->default_value(port_default), "server port")
      ("daemonize,d", po::bool_switch(&daemonize), "daemonize the server")
      ("methylome-dir,m", value(&methylome_dir)->required(), "methylome directory")
      ("max-resident,r",
       value(&max_resident)->default_value(max_resident_default),
       "max resident methylomes")
      ("threads,t", value(&n_threads)->default_value(n_threads_default),
       "number of threads")
      ("log-level,v", value(&log_level)->default_value(log_level_default),
       "log level {debug,info,warning,error,critical}")
      ("log-file,l", value(&log_filename)->value_name("console"),
       "log file name")
      // clang-format on
      ;
    return opts;
  }
};

auto
server_interface_main(int argc, char *argv[]) -> int {
  static constexpr auto usage = "Usage: mxe server [options]";
  static const auto description = "server";

  server_interface_argset args;
  std::error_code ec = args.parse(argc, argv, usage);
  if (ec == argument_error::help_requested)
    return EXIT_SUCCESS;
  if (ec)
    return EXIT_FAILURE;

  shared_ptr<ostream> log_file =
    args.log_filename.empty()
      ? make_shared<ostream>(cout.rdbuf())
      : make_shared<ofstream>(args.log_filename, std::ios::app);

  logger &lgr = logger::instance(log_file, description, args.log_level);
  if (!lgr) {
    println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  args.log_options();

  args.methylome_dir = get_canonical_dir(args.methylome_dir);
  // error messages done already
  if (args.methylome_dir.empty())
    return EXIT_FAILURE;

  if (args.daemonize) {
    server s(args.hostname, args.port, args.n_threads, args.methylome_dir,
             args.max_resident, lgr, ec);
    if (ec) {
      lgr.error("Failure daemonizing server: {}.", ec);
      return EXIT_FAILURE;
    }
    s.run();
  }
  else {
    server s(args.hostname, args.port, args.n_threads, args.methylome_dir,
             args.max_resident, lgr);
    s.run();
  }

  return EXIT_SUCCESS;
}
