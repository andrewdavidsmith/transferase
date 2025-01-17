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

namespace transferase {

struct command_config_argset : argset_base<command_config_argset> {
  static constexpr auto default_config_filename =
    "transferase_client_config.toml";

  [[nodiscard]] static auto
  get_default_config_file_impl() -> std::string {
    std::error_code ec;
    const auto config_dir = get_transferase_config_dir_default(ec);
    if (ec)
      return {};
    return std::filesystem::path(config_dir) / default_config_filename;
  }

  [[nodiscard]] static auto
  get_default_config_dir() -> std::string {
    std::error_code ec;
    const auto config_dir = get_transferase_config_dir_default(ec);
    if (ec)
      return {};
    return config_dir;
  }

  static constexpr auto port_default{"5000"};
  static constexpr auto log_level_default{log_level_t::info};
  std::string hostname{};
  std::string port{};
  std::string index_dir{};
  std::string methylome_dir{};
  std::string log_filename{};
  std::string genomes{};
  log_level_t log_level{};
  bool quiet{};

  auto
  log_options_impl() const {}

  [[nodiscard]] auto
  set_opts_impl() -> boost::program_options::options_description {
    namespace po = boost::program_options;
    using po::value;
    boost::program_options::options_description opts("Options");
    skip_parsing_config_file = true;
    // clang-format off
    opts.add_options()
      ("help,h", "print this message and exit")
      ("config-file,c", value(&config_file)
       ->value_name("[arg]")->default_value(get_default_config_file(), ""),
       "name of configuration file; see help for default")
      ("hostname,s", value(&hostname)->required(), "transferase server hostname")
      ("port,p", value(&port)->default_value(port_default), "transferase server port")
      ("genomes,g", value(&genomes),
       "attept to download index files for these genomes (comma separated list, e.g. hg38,mm39)")
      ("index-dir,x", value(&index_dir)->default_value(get_default_config_dir(), ""),
       "name of a directory to store genome index files (default: configuration directory)")
      ("methylome-dir,d", value(&methylome_dir),
       "name of a directory to search for methylomes (default: not specified)")
      ("log-level,v", value(&log_level)->default_value(log_level_default, ""),
       "{debug, info, warning, error, critical}")
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
  index_dir,
  methylome_dir,
  log_filename,
  log_level
))
// clang-format on

}  // namespace transferase

#endif  // SRC_COMMAND_CONFIG_ARGSET_HPP_
