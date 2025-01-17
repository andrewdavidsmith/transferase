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
The values that can be assigned in the generated config file are
listed among the options. This configuration file can be generated for
convenience, but it is required if the transferase server will be run
through systemd. No options have required values or defaults. If you
don't specify the value for an option, it will be left empty and
commented-out in the generated file. You can fill in those entries
after the file is generated.
)";

static constexpr auto examples = R"(
Examples:

xfrase server-config -c /path/to/server_config_file.toml \
    --hostname=not.kernel.org \
    --port=9000 \
    --methylome-dir=/data/methylomes \
    --index-dir=/data/indexes \
    --log-file=/var/tmp/transferase_server.log \
    --log-level=debug \
    --max-resident=128 \
    --n-threads=123456789 \
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

namespace transferase {

struct server_config_argset : argset_base<server_config_argset> {
  std::string hostname;
  std::string port;
  std::string methylome_dir;
  std::string index_dir;
  std::string log_file;
  std::string pid_file;
  transferase::log_level_t log_level{};
  std::uint32_t n_threads{};
  std::uint32_t max_resident{};
  std::string config_out;

  [[nodiscard]] auto
  set_common_opts_impl() -> boost::program_options::options_description {
    namespace po = boost::program_options;
    // ADS: (todo) find a way to check if this is empty so it is
    // ignored completely when printing help
    return po::options_description();
  }

  [[nodiscard]] auto
  set_cli_only_opts_impl() -> boost::program_options::options_description {
    namespace po = boost::program_options;
    using po::value;
    po::options_description opts("Options");
    opts.add_options()
      // clang-format off
      ("help,h", "print this message and exit")
      ("config-file,c", value(&config_out)->required(),
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
command_server_config_main(
  int argc,
  char *argv[])  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  -> int {
  static constexpr auto command = "server-config";
  static const auto usage =
    std::format("Usage: xfrase {} [options]\n", rstrip(command));
  static const auto about_msg =
    std::format("xfrase {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  transferase::server_config_argset args;
  auto ec = args.parse(argc, argv, usage, about_msg, description_msg);
  if (ec == argument_error_code::help_requested)
    return EXIT_SUCCESS;
  if (ec)
    return EXIT_FAILURE;

  return transferase::write_config_file(args) ? EXIT_FAILURE : EXIT_SUCCESS;
}
