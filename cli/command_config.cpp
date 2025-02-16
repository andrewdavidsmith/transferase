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
configure a transferase client
)";

static constexpr auto description = R"(
This command does the configuration to faciliate other commands,
reducing the number of command line arguments by putting them in
configuration file. Note that this configuration is not needed, as all
arguments can be specified on the command line and index files can be
downloaded separately. The default config directory is
'${HOME}/.config/transferase'. This command will also retrieve other
data. It will get index files that are used to accelerate queries. And
it will retrieve a file with MethBase2 metadata. If you run the
command again, the default behavior regarding downloads is to only
retrieve requested files if they are not already present. On a
subsequent configuration, you can request to re-download/update any of
the files obtained during the initial configuration.
)";

static constexpr auto examples = R"(
Examples:

xfr config -s example.com -p 5009 --genomes hg38,mm39
)";

#include "client_config.hpp"
#include "download_policy.hpp"
#include "logger.hpp"
#include "utilities.hpp"

#include <boost/program_options.hpp>

#include <cstdlib>
#include <format>
#include <iostream>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>  // for std::tuple
#include <vector>

auto
command_config_main(int argc, char *argv[]) -> int {  // NOLINT(*-c-arrays)
  static constexpr auto log_level_default{transferase::log_level_t::info};
  static constexpr auto download_policy_default{
    transferase::download_policy_t::missing};

  static constexpr auto command = "config";
  static const auto usage =
    std::format("Usage: xfr {} [options]\n", rstrip(command));
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  namespace xfr = transferase;

  xfr::client_config cfg;
  std::string genomes_arg{};
  bool quiet{false};
  bool debug{false};
  bool do_defaults{false};
  xfr::download_policy_t download_policy{};

  namespace po = boost::program_options;
  po::options_description opts("Options");
  opts.add_options()
    // clang-format off
    ("help,h", "print this message and exit")
    ("config-dir,c", po::value(&cfg.config_dir),
     "name of config directory; see help for default")
    ("hostname,s", po::value(&cfg.hostname), "transferase server hostname")
    ("port,p", po::value(&cfg.port), "transferase server port")
    ("genomes,g", po::value(&genomes_arg),
     "download index files for these genomes "
     "(comma separated list, e.g. hg38,mm39)")
    ("index-dir,x", po::value(&cfg.index_dir),
     "name of a directory to store genome index files")
    ("methylome-dir,d", po::value(&cfg.methylome_dir),
     "name of a local directory to search for methylomes")
    ("metadata-file,L", po::value(&cfg.metadata_file),
     "name of the MethBase2 metadata file")
    ("log-level,v", po::value(&cfg.log_level)->default_value(log_level_default, ""),
     "{debug, info, warning, error, critical}")
    ("log-file,l", po::value(&cfg.log_file),
     "log file name (default: console)")
    ("download,M", po::value(&download_policy)
     ->default_value(download_policy_default, ""),
     "download policy (none,missing,update,all)")
    ("default", po::bool_switch(&do_defaults), "only do the default configuration")
    ("quiet", po::bool_switch(&quiet), "only report errors")
    ("debug", po::bool_switch(&debug), "report debug information")
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

  auto &lgr = xfr::logger::instance(xfr::shared_from_cout(), command,
                                    debug   ? xfr::log_level_t::debug
                                    : quiet ? xfr::log_level_t::error
                                            : xfr::log_level_t::info);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Config dir", cfg.config_dir},
    {"Hostname", cfg.hostname},
    {"Port", cfg.port},
    {"Methylome dir", cfg.methylome_dir},
    {"Index dir", cfg.index_dir},
    {"Log file", cfg.log_file},
    {"Log level", std::format("{}", cfg.log_level)},
    // clang-format on
  };
  xfr::log_args<transferase::log_level_t::info>(args_to_log);

  std::error_code error;

  if (cfg.config_dir.empty()) {
    cfg.config_dir = xfr::client_config::get_default_config_dir(error);
    if (error) {
      lgr.error("Error obtaining config dir: {}.", error);
      return EXIT_FAILURE;
    }
  }

  xfr::client_config config(cfg.config_dir, error);
  if (error) {
    lgr.error("Error setting default config values: {}.", error);
    return EXIT_FAILURE;
  }

  const auto genomes = split_comma(genomes_arg);

  const std::string empty_sys_config_dir;
  config.install(genomes, download_policy, empty_sys_config_dir, error);
  if (error) {
    lgr.error("Configuration incomplete: {}", error);
    return EXIT_FAILURE;
  }
  lgr.debug("Completed configuration with status: {}", error);

  return EXIT_SUCCESS;
}
