/* MIT License
 *
 * Copyright (c) 2025 Andrew D Smith
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

#include "command_server_config.hpp"

static constexpr auto about = R"(
generate a configuration file for a transferase server
)";

static constexpr auto description = R"(
The configuration parameters used by the transferase server are listed among
the arguments. Values must be specified for most parameters that the server
uses. Recommended: if the configuration file will eventually be needed in a
system directory, first generate it in a user directory then copy it
there. This command will place the server configuration file in the specified
directory with a default name.
)";

static constexpr auto examples = R"(
Examples:

xfr server-config -c a_server_config_dir \
    --hostname=localhost \
    --port=5001 \
    --methylome-dir=my_methylomes \
    --index-dir=my_indexes \
    --log-file=/var/log/transferase_server.log \
    --log-level=debug \
    --max-resident=128 \
    --n-threads=16 \
    --pid-file=/var/run/TRANSFERASE_SERVER_PID
)";

#include "cli_common.hpp"
#include "logger.hpp"
#include "request.hpp"
#include "server_config.hpp"
#include "utilities.hpp"

#include "CLI11/CLI11.hpp"

#include <cstdlib>
#include <filesystem>
#include <format>
#include <map>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <system_error>

auto
command_server_config_main(int argc, char *argv[])  // NOLINT(*-c-arrays)
  -> int {
  static constexpr auto log_level_default = transferase::log_level_t::info;
  static constexpr auto command = "server-config";
  static const auto usage =
    std::format("Usage: xfr {} [options]", rstrip(command));
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  namespace xfr = transferase;

  // ADS: paths should not be made absolute here since these paths
  // will not be used in this app but instead be written as specified
  // into the config file.
  xfr::server_config cfg;
  cfg.min_bin_size = xfr::request::min_bin_size_default;
  cfg.max_intervals = xfr::request::max_intervals_default;
  cfg.log_level = log_level_default;

  CLI::App app{about_msg};
  argv = app.ensure_utf8(argv);
  app.usage(usage);
  if (argc >= 2)
    app.footer(description_msg);
  app.get_formatter()->column_width(column_width_default);
  app.get_formatter()->label("REQUIRED", "REQD");
  app.set_help_flag("-h,--help", "Print a detailed help message and exit");
  // clang-format off
  app.add_option("-c,--config-dir", cfg.config_dir,
                 "write specified configuration to this directory")
    ->required();
  app.add_option("-s,--hostname", cfg.hostname, "server hostname")
    ->required();
  app.add_option("-p,--port", cfg.port, "server port")->required();
  app.add_option("-d,--methylome-dir", cfg.methylome_dir,
                 "methylome directory")
    ->required();
  app.add_option("-x,--index-dir", cfg.index_dir,
                 "genome index file directory")
    ->required();
  app.add_option("-v,--log-level", cfg.log_level,
                 std::format("log level {}", xfr::log_level_help_str))
    ->option_text(std::format("[{}]", log_level_default))
    ->transform(CLI::CheckedTransformer(xfr::str_to_level, CLI::ignore_case));
  app.add_option("-l,--log-file", cfg.log_file,
                 "log file name");
  app.add_option("-r,--max-resident", cfg.max_resident,
                 "max methylomes resident in memory at once")
    ->required()
    ->check(CLI::Range(1, xfr::server_config::max_max_resident));
  app.add_option("-t,--n-threads", cfg.n_threads,
                 "number of threads to use (one per connection)")
    ->required()
    ->check(CLI::Range(1, xfr::server_config::max_n_threads));
  app.add_option("-b,--min-bin-size", cfg.min_bin_size,
                 "Minimum bin size for a request")
    ->default_str(std::format("{}", xfr::request::min_bin_size_default));
  app.add_option("-i,--max-intervals", cfg.max_intervals,
                 "Maximum number of intervals in a request")
    ->default_str(std::format("{}", xfr::request::max_intervals_default));
  app.add_option("-P,--pid-file", cfg.pid_file,
                 "Filename to use for the PID when daemonizing");
  // clang-format on

  if (argc < 2) {
    std::println("{}", app.help());
    return EXIT_SUCCESS;
  }
  CLI11_PARSE(app, argc, argv);

  std::error_code error;
  const auto config_file = xfr::server_config::get_config_file(cfg.config_dir);
  if (error) {
    std::println("Failed to get config file for {}: {}", cfg.config_dir, error);
    return EXIT_FAILURE;
  }

  const bool dir_exists = std::filesystem::exists(cfg.config_dir, error);
  if (error) {
    std::println("Error: {} ({})", error, cfg.config_dir);
    return EXIT_FAILURE;
  }
  if (!dir_exists) {
    std::filesystem::create_directories(cfg.config_dir, error);
    if (error) {
      std::println("Error creating directory {}: {}", cfg.config_dir, error);
      return EXIT_FAILURE;
    }
  }
  else {
    const bool is_dir = std::filesystem::is_directory(cfg.config_dir, error);
    if (error) {
      std::println("Error: {} ({})", error, cfg.config_dir);
      return EXIT_FAILURE;
    }
    if (!is_dir) {
      std::println("Error: {} is not a ({})", cfg.config_dir, error);
      return EXIT_FAILURE;
    }
  }

  const bool cfg_is_valid = cfg.validate(error);
  if (!cfg_is_valid) {
    std::println("Error: {}", error);
    return EXIT_FAILURE;
  }

  std::error_code write_err;
  cfg.write(config_file, write_err);
  if (write_err) {
    std::println("Error: {} ({})", config_file, write_err);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
