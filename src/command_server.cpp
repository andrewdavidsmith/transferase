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

#include "arguments.hpp"
#include "format_error_code.hpp"  // IWYU pragma: keep
#include "logger.hpp"
#include "request.hpp"
#include "server.hpp"
#include "utilities.hpp"

#include <boost/describe.hpp>
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

namespace transferase {

struct server_argset : argset_base<server_argset> {
  static constexpr auto default_config_filename =
    "transferase_server_config.toml";

  static auto
  get_default_config_file_impl() -> std::string {
    std::error_code ec;
    const auto config_dir = get_server_config_dir_default(ec);
    if (ec)
      return {};
    return std::filesystem::path{config_dir} / default_config_filename;
  }

  static constexpr auto log_level_default{log_level_t::info};
  static constexpr auto n_threads_default{1};
  static constexpr auto max_resident_default = 32;
  std::string hostname;
  std::string port;
  std::string methylome_dir;
  std::string index_dir;
  std::string log_file;
  std::string pid_file;
  transferase::log_level_t log_level{};
  std::uint32_t n_threads{};
  std::uint32_t max_resident{};
  bool daemonize{};

  auto
  log_options_impl() const {
    transferase::log_args<transferase::log_level_t::info>(
      std::vector<std::tuple<std::string, std::string>>{
        // clang-format off
        {"hostname", std::format("{}", hostname)},
        {"port", std::format("{}", port)},
        {"methylome_dir", std::format("{}", methylome_dir)},
        {"index_dir", std::format("{}", index_dir)},
        {"log_file", std::format("{}", log_file)},
        {"log_level", std::format("{}", log_level)},
        {"n_threads", std::format("{}", n_threads)},
        {"max_resident", std::format("{}", max_resident)},
        {"min_bin_size", std::format("{}", request::min_bin_size)},
        {"max_intervals", std::format("{}", request::max_intervals)},
        {"daemonize", std::format("{}", daemonize)},
        {"pid-file", std::format("{}", pid_file)},
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
    po::options_description opts("Command line or config file");
    opts.add_options()
      // clang-format off
      ("help,h", "print this message and exit")
      ("config-file,c",
       value(&config_file)
       ->default_value(get_default_config_file(), ""),
       "use specified config file")
      ("hostname,s", value(&hostname)->required(),
       "server hostname (required)")
      ("port,p", value(&port)->required(), "server port")
      ("methylome-dir,d", value(&methylome_dir)->required(), "methylome directory")
      ("index-dir,x", value(&index_dir)->required(), "genome index directory")
      ("max-resident,r",
       value(&max_resident)->default_value(max_resident_default),
       "max resident methylomes")
      ("n-threads,t", value(&n_threads)->default_value(n_threads_default),
       "number of threads")
      ("min-bin-size",
       value(&transferase::request::min_bin_size)
       ->default_value(transferase::request::min_bin_size_default),
       "minimum size of bins for queries")
      ("max-intervals",
       value(&transferase::request::max_intervals)
       ->default_value(transferase::request::max_intervals_default),
       "maximum number of intervals in a query")
      ("log-level,v", value(&log_level)->default_value(log_level_default),
       "{debug, info, warning, error, critical}")
      ("log-file,l", value(&log_file)->default_value("", "screen"),
       "log file name")
      ("daemonize", po::bool_switch(&daemonize),
       "daemonize the server")
      ("pid-file", value(&pid_file)->default_value("", "none"),
       "Filename to use for the PID  when daemonizing")
      // clang-format on
      ;
    return opts;
  }
};

// clang-format off
BOOST_DESCRIBE_STRUCT(server_argset, (), (
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

[[nodiscard]] static auto
check_directory(const auto &dirname, std::error_code &ec) -> std::string {
  auto &lgr = logger::instance();
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

}  // namespace transferase

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

  using transferase::check_directory;
  using transferase::log_level_t;
  using transferase::logger;

  transferase::server_argset args;
  auto ec = args.parse(argc, argv, usage, about_msg, description_msg);
  if (ec == argument_error_code::help_requested)
    return EXIT_SUCCESS;
  if (ec)
    return EXIT_FAILURE;

  if (args.daemonize && args.log_file.empty()) {
    std::println("A log file with write perms is needed to daemonize");
    return EXIT_FAILURE;
  }

  std::shared_ptr<std::ostream> log_file =
    args.log_file.empty()
      ? std::make_shared<std::ostream>(std::cout.rdbuf())
      : std::make_shared<std::ofstream>(args.log_file, std::ios::app);

  auto &lgr = logger::instance(log_file, command, args.log_level);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  args.log_options();

  const auto methylome_dir = check_directory(args.methylome_dir, ec);
  if (ec)
    return EXIT_FAILURE;

  const auto index_dir = check_directory(args.index_dir, ec);
  if (ec)
    return EXIT_FAILURE;

  if (args.daemonize) {
    args.pid_file = clean_path(args.pid_file, ec);
    if (ec) {
      lgr.error("Failed to get full PID file path {}: {}", args.pid_file, ec);
      return EXIT_FAILURE;
    }
    auto s = transferase::server(args.hostname, args.port, args.n_threads,
                                 methylome_dir, index_dir, args.max_resident,
                                 lgr, ec, args.daemonize, args.pid_file);
    if (ec) {
      lgr.error("Failure daemonizing server: {}", ec);
      return EXIT_FAILURE;
    }
    s.run();
  }
  else {
    auto s =
      transferase::server(args.hostname, args.port, args.n_threads,
                          methylome_dir, index_dir, args.max_resident, lgr, ec);
    if (ec) {
      lgr.error("Failure initializing server: {}", ec);
      return EXIT_FAILURE;
    }
    s.run();
  }

  return EXIT_SUCCESS;
}
