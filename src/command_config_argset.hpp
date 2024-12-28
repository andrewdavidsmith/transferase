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

#ifndef SRC_COMMAND_CONFIG_ARGSET_HPP_
#define SRC_COMMAND_CONFIG_ARGSET_HPP_

#include "arguments.hpp"
#include "logger.hpp"
#include "utilities.hpp"

#include <boost/describe.hpp>
#include <boost/program_options.hpp>

#include <filesystem>
#include <string>
#include <system_error>

struct command_config_argset : argset_base<command_config_argset> {
  static constexpr auto default_config_filename = "xfrase_client_config.toml";

  [[nodiscard]] static auto
  get_default_config_file_impl() -> std::string {
    std::error_code ec;
    const auto config_dir = get_xfrase_config_dir_default(ec);
    if (ec)
      return {};
    return std::filesystem::path(config_dir) / default_config_filename;
  }

  static constexpr auto port_default{"5000"};
  static constexpr auto log_level_default{xfrase_log_level::info};
  std::string hostname{};
  std::string port{};
  std::string index_dir{};
  std::string log_filename{};
  std::string assemblies{};
  xfrase_log_level log_level{};
  std::string client_config_file{};
  bool quiet{};

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
       ->value_name("[arg]")->default_value(get_default_config_file(), ""),
       "config file; see help for default")
      ("hostname,s", value(&hostname)->required(), "server hostname")
      ("port,p", value(&port)->default_value(port_default), "server port")
      ("assemblies,a", value(&assemblies),
       "get index files for these assemblies (comma sep, e.g., hg38,mm39)")
      ("log-level,v", value(&log_level)->default_value(log_level_default),
       "log level {debug,info,warning,error,critical}")
      ("log-file,l", value(&log_filename)->value_name("console"),
       "log file name")
      ("quiet,q", po::bool_switch(&quiet), "only print error info to terminal")
      ;
    // clang-format on
    return opts;
  }
};

// clang-format off
BOOST_DESCRIBE_STRUCT(command_config_argset, (), (
  hostname,
  port,
  log_filename,
  log_level
))
// clang-format on

#endif  // SRC_COMMAND_CONFIG_ARGSET_HPP_
