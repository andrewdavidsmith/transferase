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
The configuration parameters used by the transferase server are listed
among the arguments. The transferase server configuration file can be
generated for convenience, but it is required if the server will be
run through systemd. Values must be specified for all parameters
unless the 'force' argument is used, in which case any parameters
without values will be left as commented-out lines in the
configuration file. Those must be specified manually or given on the
command line when running the server. Recommended: if the
configuration file will eventually be needed in a system directory,
first generate it in a user directory then copy it there.
)";

static constexpr auto examples = R"(
Examples:

xfr server-config -c /path/to/server_config_file.toml \
    --hostname=not.kernel.org \
    --port=65539 \
    --methylome-dir=/data/methylomes \
    --index-dir=/data/indexes \
    --log-file=/var/tmp/transferase_server.log \
    --log-level=debug \
    --max-resident=128 \
    --n-threads=9000 \
    --pid-file=/var/tmp/TRANSFERASE_SERVER_PID
)";

#include "arguments.hpp"
#include "config_file_utils.hpp"
#include "logger.hpp"
#include "utilities.hpp"

#include <boost/describe.hpp>
#include <boost/program_options.hpp>

#include <cstdint>
#include <cstdlib>
#include <format>
#include <string>
#include <string_view>
#include <system_error>

#include <print>
#include <sstream>
#include <tuple>
#include <variant>  // for std::tuple
#include <vector>

namespace transferase {

struct server_config_argset : argset_base<server_config_argset> {
  [[nodiscard]] static auto
  get_default_config_file_impl() -> std::string {
    // skpping parsing config file
    return {};
  }

  std::string hostname;
  std::string port;
  std::string methylome_dir;
  std::string index_dir;
  std::string log_file;
  std::string pid_file;
  std::string log_level;
  std::string n_threads;
  std::string max_resident;
  bool force{false};

  auto
  log_options_impl() const {
    transferase::log_args<log_level_t::info>(
      std::vector<std::tuple<std::string, std::string>>{
        // clang-format off
        {"config_file", std::format("{}", config_file)},
        {"hostname", std::format("{}", hostname)},
        {"port", std::format("{}", port)},
        {"methylome_dir", std::format("{}", methylome_dir)},
        {"index_dir", std::format("{}", index_dir)},
        {"log_file", std::format("{}", log_file)},
        {"log_level", std::format("{}", log_level)},
        {"n_threads", std::format("{}", n_threads)},
        {"max_resident", std::format("{}", max_resident)},
        {"pid_file", std::format("{}", pid_file)},
        // clang-format on
      });
  }

  [[nodiscard]] auto
  set_hidden_impl() -> boost::program_options::options_description {
    return {};
  }

  [[nodiscard]] auto
  set_opts_impl() -> boost::program_options::options_description {
    namespace po = boost::program_options;
    using po::value;
    skip_parsing_config_file = true;
    po::options_description opts("Options");
    opts.add_options()
      // clang-format off
      ("help,h", "print this message and exit")
      ("config-file,c", value(&config_file)->required(),
       "write specified configuration to this file")
      ("hostname,s", value(&hostname), "server hostname")
      ("port,p", value(&port), "server port")
      ("methylome-dir,d", value(&methylome_dir), "methylome directory")
      ("index-dir,x", value(&index_dir), "genome index file directory")
      ("max-resident,r", value(&max_resident), "max methylomes resident in memory at once")
      ("n-threads,t", value(&n_threads), "number of threads to use (one per connection)")
      ("log-level,v", value(&log_level), "{debug, info, warning, error, critical}")
      ("log-file,l", value(&log_file), "log file name")
      ("pid-file,p", value(&pid_file), "Filename to use for the PID when daemonizing")
      ("force", po::bool_switch(&force), "Write config file even if values needed to "
       "run the server are missing (set them manually)")
      // clang-format on
      ;
    return opts;
  }
};

// clang-format off
BOOST_DESCRIBE_STRUCT(server_config_argset, (), (
  hostname,
  port,
  methylome_dir,
  index_dir,
  log_file,
  log_level,
  n_threads,
  max_resident,
  pid_file
)
)
// clang-format on

}  // namespace transferase

auto
command_server_config_main(int argc,
                           char *argv[]) -> int {  // NOLINT(*-c-arrays)
  static constexpr auto invalid_fmt = R"(Warning: {} is not valid for "{}")";
  static constexpr auto command = "server-config";
  static const auto usage =
    std::format("Usage: xfr {} [options]\n", rstrip(command));
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  transferase::server_config_argset args;
  auto ec = args.parse(argc, argv, usage, about_msg, description_msg);
  if (ec == argument_error_code::help_requested)
    return EXIT_SUCCESS;
  if (ec)
    return EXIT_FAILURE;

  if (!args.force) {
    const auto missing = check_empty_values(args);
    if (!missing.empty()) {
      std::println("The following have missing values (constider --force):");
      for (const auto &m : missing)
        std::println("{}", m);
      return EXIT_FAILURE;
    }
  }

  // validate n-threads
  if (!args.n_threads.empty()) {
    std::istringstream is(args.n_threads);
    std::uint16_t n_threads_value{};
    if (!(is >> n_threads_value))
      std::println(invalid_fmt, args.n_threads, "n-threads");
  }

  // validate port
  if (!args.port.empty()) {
    std::istringstream is(args.port);
    std::uint16_t port_value{};
    if (!(is >> port_value))
      std::println(invalid_fmt, args.port, "port");
  }

  // validate log-level
  if (!args.log_level.empty()) {
    std::istringstream is(args.log_level);
    transferase::log_level_t log_level_value{};
    if (!(is >> log_level_value))
      std::println(invalid_fmt, args.log_level, "log-level");
  }

  // validate max-resident
  if (!args.max_resident.empty()) {
    std::istringstream is(args.max_resident);
    std::uint32_t max_resident_value{};
    if (!(is >> max_resident_value))
      std::println(invalid_fmt, args.max_resident, "max-resident");
  }

  const auto write_err = transferase::write_config_file(args);
  if (write_err)  // message already reported
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}
