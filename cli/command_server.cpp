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

#include "command_server.hpp"

static constexpr auto about = R"(
start an transferase server
)";

static constexpr auto description = R"(
A transferase server transfers methylation features to clients. The
server must be provided with one directory for methylomes and one
directory for genome indexes. The methylome directory must include pairs
of methylome data and metadata files as produced by the 'format'
command. The indexes directory must include pairs of genome index data
and metadata files as produced by the 'index' command. For each
methylome in the methylomes directory, the corresponding index must be
present in the indexes directory. For example, if a methylome was
analyzed using human reference hg38, then an index for hg38 must be
available. Note: the hostname or ip address for the server needs to be
used exactly by the client. If the server is started using 'localhost'
as the hostname, it will not be reachable by any remote client. The
server can run in detached mode.
)";

static constexpr auto examples = R"(
Examples:

xfr server -s localhost -d methylomes -x indexes
)";

#include "format_error_code.hpp"  // IWYU pragma: keep
#include "logger.hpp"
#include "request.hpp"
#include "server.hpp"
#include "server_config.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <cstdint>
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>  // std::make_shared
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>
#include <vector>

[[nodiscard]] static auto
check_directory(const auto &dirname, std::error_code &ec) -> std::string {
  auto &lgr = transferase::logger::instance();
  const auto canonical_dir = std::filesystem::canonical(dirname, ec);
  if (ec || canonical_dir.empty()) {
    lgr.error("Failed to get canonical directory for {}: {}", dirname, ec);
    return {};
  }
  const auto is_dir = std::filesystem::is_directory(canonical_dir, ec);
  if (ec) {
    lgr.error("Failed to identify directory {}: {}", canonical_dir.string(),
              ec);
    return {};
  }
  if (!is_dir) {
    ec = std::make_error_code(std::errc::not_a_directory);
    lgr.error("{}: {}", canonical_dir.string(), ec);
    return {};
  }
  return canonical_dir;
}

auto
command_server_main(int argc,
                    char *argv[])  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  -> int {
  static constexpr auto command = "server";
  static const auto usage =
    std::format("Usage: xfr {} [options]\n", rstrip(command));
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  namespace xfr = transferase;

  xfr::server_config cfg;
  std::string config_file;
  bool daemonize{};

  // get the default config directory to use as a fallback
  std::error_code default_config_dir_error;
  const std::string default_config_dir =
    xfr::server_config::get_default_config_dir(default_config_dir_error);
  if (default_config_dir_error) {
    std::println("Failed to identify default config dir: {}",
                 default_config_dir_error);
    return EXIT_FAILURE;
  }

  namespace po = boost::program_options;

  po::options_description opts("Options");
  opts.add_options()
    // clang-format off
    ("help,h", "print this message and exit")
    ("config-file,c", po::value(&config_file),
     "read configuration from this file")
    ("hostname,s", po::value(&cfg.hostname), "server hostname")
    ("port,p", po::value(&cfg.port), "server port")
    ("methylome-dir,d", po::value(&cfg.methylome_dir), "methylome directory")
    ("index-dir,x", po::value(&cfg.index_dir), "genome index directory")
    ("max-resident,r", po::value(&cfg.max_resident), "max resident methylomes")
    ("n-threads,t", po::value(&cfg.n_threads), "number of threads")
    ("min-bin-size", po::value(&xfr::request::min_bin_size),
     "minimum size of bins for queries")
    ("max-intervals", po::value(&xfr::request::max_intervals),
     "maximum number of intervals in a query")
    ("log-level,v", po::value(&cfg.log_level),
     "{debug, info, warning, error, critical}")
    ("log-file,l", po::value(&cfg.log_file), "log file name")
    ("daemonize", po::bool_switch(&daemonize), "daemonize the server")
    ("pid-file", po::value(&cfg.pid_file)->default_value("", "none"),
     "Filename to use for the PID  when daemonizing")
    // clang-format on
    ;
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, opts), vm);
    if (vm.count("help") || argc == 1) {
      std::println("{}\n{}", about_msg, usage);
      opts.print(std::cout);
      std::println("\n{}", description_msg);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    std::println("{}", e.what());
    return EXIT_FAILURE;
  }

  // Attempting to load values from config file in cfg.config_file but
  // deferring error reporting as all values might have been specified
  // on the command line.
  std::error_code error;
  if (!config_file.empty()) {
    cfg.read_config_file(config_file, error);
    if (error) {
      std::println("Failed to read config file {}: {}", config_file, error);
      return EXIT_FAILURE;
    }
  }

  if (daemonize && cfg.log_file.empty()) {
    std::println("A log file with write perms is needed to daemonize");
    return EXIT_FAILURE;
  }

  std::shared_ptr<std::ostream> log_file =
    cfg.log_file.empty()
      ? std::make_shared<std::ostream>(std::cout.rdbuf())
      : std::make_shared<std::ofstream>(cfg.log_file, std::ios::app);

  auto &lgr = xfr::logger::instance(log_file, command, cfg.log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Config file", config_file},
    {"Hostname", cfg.hostname},
    {"Port", cfg.port},
    {"Methylome dir", cfg.methylome_dir},
    {"Index dir", cfg.index_dir},
    {"Log file", cfg.log_file},
    {"Pid file", cfg.pid_file},
    {"Log level", std::format("{}", cfg.log_level)},
    {"N threads", std::format("{}", cfg.n_threads)},
    {"Max resident", std::format("{}", cfg.max_resident)},
    {"Min bin size", std::format("{}", cfg.min_bin_size)},
    {"Max intervals", std::format("{}", cfg.max_intervals)},
    // clang-format on
  };

  xfr::log_args<transferase::log_level_t::info>(args_to_log);

  const bool cfg_is_valid = cfg.validate(error);
  if (!cfg_is_valid || error) {
    lgr.error("Invalid server configuration: {}", error);
    return EXIT_FAILURE;
  }

  const auto methylome_dir = check_directory(cfg.methylome_dir, error);
  if (error)
    return EXIT_FAILURE;

  const auto index_dir = check_directory(cfg.index_dir, error);
  if (error)
    return EXIT_FAILURE;

  if (daemonize) {
    auto s = xfr::server(cfg.hostname, cfg.port, cfg.n_threads, methylome_dir,
                         index_dir, cfg.max_resident, lgr, error, daemonize,
                         cfg.pid_file);
    if (error) {
      lgr.error("Failure daemonizing server: {}", error);
      return EXIT_FAILURE;
    }
    s.run();
  }
  else {
    auto s = xfr::server(cfg.hostname, cfg.port, cfg.n_threads, methylome_dir,
                         index_dir, cfg.max_resident, lgr, error);
    if (error) {
      lgr.error("Failure initializing server: {}", error);
      return EXIT_FAILURE;
    }
    s.run();
  }

  return EXIT_SUCCESS;
}
