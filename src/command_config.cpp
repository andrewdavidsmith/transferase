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

#include "command_config.hpp"

static constexpr auto about = R"(
configure an mxe client
)";

static constexpr auto description = R"(
This command does the configuration to faciliate commands running in
mode. In particular, this command creates a configuration file that
reduces the amount of information a user needs to provide on command
lines. It also will retrieve index files that are needed to accelerate
queries. Note that this configuration is not needed, as all arguments
can be specified on the command line and index files can be downloaded
separately. The default config directory is
'${HOME}/.config/transferase'.
)";

static constexpr auto examples = R"(
Examples:

mxe config -s example.com -p 5009 --assemblies hg38 mm39
)";

#include "arguments.hpp"
#include "config_file_utils.hpp"  // write_client_config_file
#include "logger.hpp"
#include "mxe_error.hpp"  // IWYU pragma: keep
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
#include <variant>  // IWYU pragma: keep
#include <vector>

struct config_argset : argset_base<config_argset> {
  static constexpr auto default_config_filename = "mxe_client_config.toml";

  [[nodiscard]] static auto
  get_default_config_file_impl() -> std::string {
    std::error_code ec;
    const auto config_dir = get_mxe_config_dir_default(ec);
    if (ec)
      return {};
    return std::filesystem::path(config_dir) / default_config_filename;
  }

  static constexpr auto port_default{"5000"};
  static constexpr auto log_level_default{mxe_log_level::info};
  std::string hostname{};
  std::string port{};
  std::string index_dir{};
  std::string log_filename{};
  mxe_log_level log_level{};
  std::string client_config_file{};
  bool verbose{};

  auto
  log_options_impl() const {}

  [[nodiscard]] auto
  set_common_opts_impl() -> boost::program_options::options_description {
    return {};
  }

  [[nodiscard]] auto
  set_cli_only_opts_impl() -> boost::program_options::options_description {
    namespace po = boost::program_options;
    using po::value;
    boost::program_options::options_description opts("Command line only");
    // clang-format off
    opts.add_options()
      ("help,h", "print this message and exit")
      ("config-file,c", value(&client_config_file)
       ->value_name("[arg]")->implicit_value(get_default_config_file(), ""),
       "use this config file")
      ("hostname,s", value(&hostname)->required(),
       "server hostname")
      ("port,p", value(&port)->default_value(port_default), "server port")
      ("assemblies,a", po::value<std::vector<std::string>>()->multitoken(),
       "get index files for these assemblies (e.g., hg38)")
      ("log-level,v", value(&log_level)->default_value(log_level_default),
       "log level {debug,info,warning,error,critical}")
      ("log-file,l", value(&log_filename)->value_name("console"),
       "log file name")
      ("verbose", po::bool_switch(&verbose), "print info to terminal")
      ;
    // clang-format on
    return opts;
  }
};

// clang-format off
BOOST_DESCRIBE_STRUCT(config_argset, (), (
  hostname,
  port,
  log_filename,
  log_level
)
)
// clang-format on

auto
command_config_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "config";
  static const auto usage =
    std::format("Usage: mxe {} [options]\n", strip(command));
  static const auto about_msg =
    std::format("mxe {}: {}", strip(command), strip(about));
  static const auto description_msg =
    std::format("{}\n{}", strip(description), strip(examples));

  config_argset args;
  auto ec = args.parse(argc, argv, usage, about_msg, description_msg);
  if (ec == argument_error::help_requested)
    return EXIT_SUCCESS;
  if (ec)
    return EXIT_FAILURE;

  std::shared_ptr<std::ostream> log_file =
    args.log_filename.empty()
      ? std::make_shared<std::ostream>(std::cout.rdbuf())
      : std::make_shared<std::ofstream>(args.log_filename, std::ios::app);

  if (args.verbose)
    std::ranges::for_each(
      std::vector<std::tuple<std::string, std::string>>{
        // clang-format off
      {"config_file", std::format("{}", args.client_config_file)},
      {"hostname", std::format("{}", args.hostname)},
      {"port", std::format("{}", args.port)},
      {"log_filename", std::format("{}", args.log_filename)},
      {"log_level", std::format("{}", args.log_level)},
        // clang-format on
      },
      [](auto &&x) { std::println("{}: {}", std::get<0>(x), std::get<1>(x)); });

  args.client_config_file =
    std::filesystem::absolute(args.client_config_file, ec);
  args.client_config_file =
    std::filesystem::weakly_canonical(args.client_config_file, ec);
  if (ec) {
    std::println("Bad config file {}: {}", args.client_config_file, ec);
    return EXIT_FAILURE;
  }

  const auto config_dir =
    std::filesystem::path(args.client_config_file).parent_path();
  if (args.verbose)
    std::println("Client config directory: {}", config_dir);

  {
    const bool dir_exists = std::filesystem::exists(config_dir, ec);
    if (dir_exists && !std::filesystem::is_directory(config_dir, ec)) {
      std::println("File exists and is not a directory: {}", config_dir);
      return EXIT_FAILURE;
    }
    if (!dir_exists) {
      if (args.verbose)
        std::println("Creating directory {}", config_dir);
      const bool made_dir = std::filesystem::create_directories(config_dir, ec);
      if (!made_dir) {
        std::println("{}: {}", config_dir, ec);
        return EXIT_FAILURE;
      }
    }
  }

  // take care of obtaining index files

  return write_client_config_file(args) ? EXIT_FAILURE : EXIT_SUCCESS;
}
