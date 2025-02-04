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
#include "client_config.hpp"
#include "logger.hpp"

#include <boost/program_options.hpp>

#include <format>
#include <string>
#include <tuple>
#include <variant>  // for std::tuple
#include <vector>

namespace transferase {

struct command_config_argset : argset_base<command_config_argset> {
  [[nodiscard]] static auto
  get_default_config_file_impl() -> std::string {
    // skpping parsing config file
    return {};
  }

  static constexpr auto port_default{"5000"};
  static constexpr auto log_level_default{log_level_t::info};

  std::string config_dir{};
  std::string genomes{};

  client_config config;

  bool update{};
  bool quiet{};
  bool debug{};
  bool force_download{};

  auto
  log_options_impl() const {
    transferase::log_args<log_level_t::info>(
      std::vector<std::tuple<std::string, std::string>>{
        // clang-format off
        {"config_dir", std::format("{}", config_dir)},
        {"hostname", std::format("{}", config.hostname)},
        {"port", std::format("{}", config.port)},
        {"index_dir", std::format("{}", config.index_dir)},
        {"labels_dir", std::format("{}", config.labels_dir)},
        {"methylome_dir", std::format("{}", config.methylome_dir)},
        {"log_file", std::format("{}", config.log_file)},
        {"log_level", std::format("{}", config.log_level)},
        {"genomes", std::format("{}", genomes)},
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
    boost::program_options::options_description opts("Options");
    // clang-format off
    opts.add_options()
      ("help,h", "print this message and exit")
      ("config-dir,c", value(&config_dir),
       "name of config directory; see help for default")
      ("hostname,s", value(&config.hostname), "transferase server hostname")
      ("port,p", value(&config.port)->default_value(port_default), "transferase server port")
      ("genomes,g", value(&genomes),
       "download index files and sample labels for these genomes "
       "(comma separated list, e.g. hg38,mm39)")
      ("index-dir,x", value(&config.index_dir),
       "name of a directory to store genome index files")
      ("methylome-dir,d", value(&config.methylome_dir),
       "name of a local directory to search for methylomes")
      ("labels-dir,L", value(&config.labels_dir),
       "name of a directory to search for sample labels")
      ("log-level,v", value(&config.log_level)->default_value(log_level_default, ""),
       "{debug, info, warning, error, critical}")
      ("log-file,l", value(&config.log_file)->value_name("console"),
       "log file name")
      ("update,u", po::bool_switch(&update), "update sample labels")
      ("force,f", po::bool_switch(&force_download), "force index file update")
      ("quiet", po::bool_switch(&quiet), "only report on errors")
      ("debug", po::bool_switch(&debug), "report debug information")
      ;
    // clang-format on
    return opts;
  }
};

}  // namespace transferase

#endif  // SRC_COMMAND_CONFIG_ARGSET_HPP_
