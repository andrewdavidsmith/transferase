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
among the arguments. Values must be specified for most parameters that
the server uses. Recommended: if the configuration file will
eventually be needed in a system directory, first generate it in a
user directory then copy it there. This command will place the server
configuration file in the specified directory with a default name.
)";

static constexpr auto examples = R"(
Examples:

xfr server-config -c /path/to/server_config_file.conf \
    --hostname=org.kernel.not \
    --port=65536 \
    --methylome-dir=/data/methylomes \
    --index-dir=/data/indexes \
    --log-file=/var/tmp/transferase_server.log \
    --log-level=debug \
    --max-resident=1984 \
    --n-threads=9000 \
    --pid-file=/var/tmp/TRANSFERASE_SERVER_PID
)";

#include "config_file_utils.hpp"
#include "logger.hpp"
#include "request.hpp"
#include "server_config.hpp"
#include "utilities.hpp"

#include <boost/describe.hpp>
#include <boost/mp11/algorithm.hpp>  // for boost::mp11::mp_for_each
#include <boost/program_options.hpp>

#include <algorithm>  // for std::ranges::replace
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iterator>  // for std::cend
#include <print>
#include <ranges>  // for std::ranges::find
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>  // for std::remove_cvref
#include <variant>      // for std::tuple
#include <vector>

[[nodiscard]] inline auto
check_empty_values(const auto &t, const std::vector<std::string> &allowed_empty)
  -> std::vector<std::string> {
  using T = std::remove_cvref_t<decltype(t)>;
  using members =
    boost::describe::describe_members<T, boost::describe::mod_any_access>;
  std::vector<std::string> r;
  boost::mp11::mp_for_each<members>([&](const auto &member) {
    std::string name(member.name);
    std::ranges::replace(name, '_', '-');
    const auto value = std::format("{}", t.*member.pointer);
    const auto itr = std::ranges::find(allowed_empty, name);
    if (value.empty() && itr == std::cend(allowed_empty))
      r.push_back(name);
  });
  return r;
}

auto
command_server_config_main(int argc, char *argv[])  // NOLINT(*-c-arrays)
  -> int {
  static constexpr auto command = "server-config";
  static const auto usage =
    std::format("Usage: xfr {} [options]\n", rstrip(command));
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  namespace xfr = transferase;

  xfr::server_config cfg;

  namespace po = boost::program_options;

  po::options_description opts("Options");
  opts.add_options()
    // clang-format off
    ("help,h", "print this message and exit")
    ("config-dir,c", po::value(&cfg.config_dir)->required(),
     "write specified configuration to this directory")
    ("hostname,s", po::value(&cfg.hostname)->required(), "server hostname")
    ("port,p", po::value(&cfg.port)->required(), "server port")
    ("methylome-dir,d", po::value(&cfg.methylome_dir)->required(),
     "methylome directory")
    ("index-dir,x", po::value(&cfg.index_dir)->required(),
     "genome index file directory")
    ("log-level,v", po::value(&cfg.log_level)
     ->default_value(xfr::log_level_t::debug), "{debug, info, warning, error, critical}")
    ("log-file,l", po::value(&cfg.log_file)->required(), "log file name")
    ("max-resident,r", po::value(&cfg.max_resident)->required(),
     "max methylomes resident in memory at once")
    ("n-threads,t", po::value(&cfg.n_threads)->required(),
     "number of threads to use (one per connection)")
    ("min-bin-size,b", po::value(&cfg.min_bin_size)
     ->default_value(xfr::request::min_bin_size_default), "Minimum bin size for a request")
    ("max-intervals,i", po::value(&cfg.max_intervals)
     ->default_value(xfr::request::max_intervals_default), "Maximum number of intervals in a request")
    ("pid-file,P", po::value(&cfg.pid_file), "Filename to use for the PID when daemonizing")
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

  std::error_code error;
  const auto config_file =
    xfr::server_config::get_config_file(cfg.config_dir, error);
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

  const auto write_err = transferase::write_config_file(cfg, config_file);
  if (write_err) {
    std::println("Error writing config file {}: {}", config_file, write_err);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
