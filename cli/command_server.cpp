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
start a transferase server
)";

static constexpr auto description = R"(
Start a transferase server instance. The server must be provided with one
directory for methylomes and one directory for genome indexes. The methylome
directory must include pairs of methylome data and metadata files as produced
by the 'format' command. The indexes directory must include pairs of genome
index data and metadata files as produced by the 'index' command. For each
methylome in the methylomes directory, the corresponding index must be present
in the indexes directory. For example, if a methylome was analyzed using human
reference hg38, then an index for hg38 must be available. Note: the hostname
or ip address for the server needs to be used exactly by the client. If the
server is started using 'localhost' as the hostname, it will not be reachable
by any remote client. The server can run in detached mode.
)";

static constexpr auto examples = R"(
Examples:

xfr server -s localhost -d methylomes -x indexes
)";

#include "cli_common.hpp"
#include "format_error_code.hpp"  // IWYU pragma: keep
#include "logger.hpp"
#include "request.hpp"
#include "server.hpp"
#include "server_config.hpp"
#include "utilities.hpp"

#include "CLI11/CLI11.hpp"

#include <config.h>

#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
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
command_server_main(int argc, char *argv[]) -> int {  // NOLINT(*-c-arrays)
  static constexpr auto log_level_default = transferase::log_level_t::debug;
  static constexpr auto command = "server";
  static const auto usage =
    std::format("Usage: xfr {} [options]", rstrip(command));
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  namespace xfr = transferase;

  xfr::server_config cfg;
  cfg.min_bin_size = xfr::request::min_bin_size_default;
  cfg.max_intervals = xfr::request::max_intervals_default;
  cfg.log_level = log_level_default;
  cfg.max_resident = xfr::server_config::default_max_resident;
  cfg.n_threads = xfr::server_config::default_n_threads;
  std::string config_file;
  bool daemonize{false};

  CLI::App app{about_msg};
  argv = app.ensure_utf8(argv);
  app.usage(usage);
  if (argc >= 2)
    app.footer(description_msg);
  app.get_formatter()->column_width(column_width_default);
  app.get_formatter()->label("REQUIRED", "REQD");
  app.set_help_flag("-h,--help", "Print a detailed help message and exit");
  // clang-format off
  app.add_option("-c,--config-file", config_file,
                 "read configuration from this file")
    ->check(CLI::ExistingFile);
  app.add_option("-s,--hostname", cfg.hostname, "server hostname");
  app.add_option("-p,--port", cfg.port, "server port");
  app.add_option("-d,--methylome-dir", cfg.methylome_dir, "methylome directory")
    ->check(CLI::ExistingDirectory);
  app.add_option("-x,--index-dir", cfg.index_dir, "genome index directory")
    ->check(CLI::ExistingDirectory);
  app.add_option("-r,--max-resident", cfg.max_resident, "max resident methylomes");
  app.add_option("-t,--n-threads", cfg.n_threads, "number of threads");
  app.add_option("--min-bin-size", xfr::request::min_bin_size,
                 "minimum size of bins for queries");
  app.add_option("--max-intervals", xfr::request::max_intervals,
                 "maximum number of intervals in a query");
  app.add_option("-v,--log-level", cfg.log_level,
                 std::format("log level {}", xfr::log_level_help_str))
    ->option_text(std::format("[{}]", log_level_default))
    ->transform(CLI::CheckedTransformer(xfr::str_to_level, CLI::ignore_case));
  app.add_option("-l,--log-file", cfg.log_file,
                 "log file name");
  app.add_option("--pid-file", cfg.pid_file,
                 "Filename to use for the PID  when daemonizing");
  app.add_flag("--daemonize", daemonize, "daemonize the server");
  // clang-format on

  if (argc < 2) {
    std::println("{}", app.help());
    return EXIT_SUCCESS;
  }
  CLI11_PARSE(app, argc, argv);

  // Attempting to load values from config file in cfg.config_file but
  // deferring error reporting as all values might have been specified
  // on the command line.
  std::error_code error;
  if (!config_file.empty()) {
    // make any assigned paths absolute so that subsequent composition
    // with any config_dir will not overwrite any relative path
    // specified on the command line.
    cfg.make_paths_absolute();
    cfg.read_config_file_no_overwrite(config_file, error);
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

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Config file", config_file},
    {"VERSION", VERSION},
    {"Version from config file", cfg.version},
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

  if (cfg.version != VERSION)
    lgr.warning("Version ({}) not the same as found in config file ({})",
                VERSION, cfg.version);

  xfr::log_args<transferase::log_level_t::info>(args_to_log);

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
