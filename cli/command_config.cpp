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
Configure transferase on your system. The default config directory is
'${HOME}/.config/transferase'. This command will also retrieve other data. It
will get index files that are used to accelerate queries. And it will retrieve
files with MethBase2 metadata. This command has modes that allow you to
update an existing configuration or reset a configuration to default
values. Note: configuration is not strictly needed, as most other commands can
run with all information provided on the command line.
)";

static constexpr auto examples = R"(
Examples:

xfr config --defaults

xfr config --genomes hg38,mm39

xfr config -s example.com -p 5009 --genomes hg38,mm39

xfr config --update -s localhost -p 5000
)";

#include "cli_common.hpp"
#include "client_config.hpp"
#include "download_policy.hpp"
#include "logger.hpp"
#include "utilities.hpp"

#include "macos_helper.hpp"

#include "CLI11/CLI11.hpp"

#include <cstdlib>
#include <exception>
#include <format>
#include <map>
#include <memory>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>  // for std::tuple
#include <vector>

auto
command_config_main(int argc, char *argv[]) -> int {  // NOLINT(*-c-arrays)
  static constexpr auto log_level_default = transferase::log_level_t::info;
  static constexpr auto download_policy_default =
    transferase::download_policy_t::missing;

  static constexpr auto command = "config";
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
  xfr::client_config cfg;
  cfg.log_level = log_level_default;
  std::vector<std::string> genomes;
  bool quiet{false};
  bool debug{false};
  bool update_config{false};
  bool no_defaults{false};
  bool all_defaults{false};
  bool show_progress{false};
  xfr::download_policy_t download_policy{download_policy_default};

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
                 "name of config directory; see help for default")
    ->option_text("TEXT")
    ->check(!CLI::ExistingFile);
  app.add_option("-s,--hostname", cfg.hostname, "transferase server hostname");
  app.add_option("-p,--port", cfg.port, "transferase server port");
  app.add_option("-x,--index-dir", cfg.index_dir,
                 "name of a directory to store genome index files");
  app.add_option("--metadata-dataframe", cfg.metadata_dataframe,
                 "name of the MethBase2 metadata dataframe");
  app.add_option("--methylome-list", cfg.methylome_list,
                 "name of the methylome list (for a remote or local server)");
  app.add_option("--select-metadata", cfg.select_metadata,
                 "name of the 'select' metadata");
  app.add_option("-d,--methylome-dir", cfg.methylome_dir,
                 "name of a local directory to search for methylomes");
  app.add_option("-v,--log-level", cfg.log_level,
                 "{debug, info, warning, error, critical}")
    ->option_text(std::format("ENUM [{}]", log_level_default))
    ->transform(CLI::CheckedTransformer(xfr::str_to_level, CLI::ignore_case));
  app.add_option("-g,--genomes", genomes,
                 "download index files for these genomes "
                 "(comma separated list, e.g. hg38,mm39)")
    ->delimiter(',');
  app.add_option("--download", download_policy,
                 "download policy (none, missing, update, all) " +
                 std::format("default: {}", download_policy_default))
    ->option_text("ENUM")
    ->transform(CLI::CheckedTransformer(xfr::download_policy_cli11, CLI::ignore_case));
  const auto all_defaults_opt =
    app.add_flag("--defaults", all_defaults, "allow all default config values")
    ->option_text(" ");
  app.add_flag("--no-defaults", no_defaults, "use no defaults for missing values")
    ->option_text(" ")
    ->excludes(all_defaults_opt);
  app.add_flag("--update", update_config, "keep values previously configured");
  const auto quiet_opt =
    app.add_flag("--quiet", quiet, "only report errors")
    ->option_text(" ");
  app.add_flag("--progress", show_progress, "show download progress")
    ->option_text(" ")
    ->excludes(quiet_opt);
  app.add_flag("--debug", debug, "report debug information")
    ->option_text(" ")
    ->excludes(quiet_opt);
  // clang-format on

  if (argc < 2) {
    std::println("{}", app.help());
    return EXIT_SUCCESS;
  }
  CLI11_PARSE(app, argc, argv);

  auto &lgr = xfr::logger::instance(xfr::shared_from_cout(), command,
                                    debug   ? xfr::log_level_t::debug
                                    : quiet ? xfr::log_level_t::error
                                            : xfr::log_level_t::info);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  std::error_code error;

  // get the config dir if it's empty
  if (cfg.config_dir.empty()) {
    cfg.config_dir = xfr::client_config::get_default_config_dir(error);
    if (error) {
      lgr.error("Error obtaining config dir: {}", error);
      return EXIT_FAILURE;
    }
    lgr.debug("Taking default value for config dir: {}", cfg.config_dir);
  }

  // If the user is requesting not to Unless the users is load any previously
  // set values in the same config directory
  if (cfg.config_file_exists()) {
    lgr.debug("Loading unspecified values from previous config file: {}",
              cfg.get_config_file(cfg.config_dir));
    auto tmp_cfg = cfg;
    tmp_cfg.read_config_file_no_overwrite(error);
    if (error) {
      lgr.info("Existing config is invalid and will be replaced");
      std::filesystem::remove(cfg.get_config_file(cfg.config_dir), error);
    }
    else
      cfg = tmp_cfg;
  }

  // If the user is allowing defeaults, any args are left unspecified
  // that can be defaulted are.
  const std::string empty_sys_config_dir;
  if (!no_defaults) {
    lgr.debug("Assigning defaults to remaining unspecified required values");
    cfg.assign_defaults_to_missing(empty_sys_config_dir, error);
  }

  // const auto genomes_joined =
  //   genomes | std::views::join_with(',') | std::ranges::to<std::string>();
  const auto genomes_joined = join_with(genomes, ',');

  using std::string_literals::operator""s;
  constexpr auto or_none = [](const auto &s) {
    return s.empty() ? "none specified"s : s;
  };
  const std::vector<std::tuple<std::string, std::string>> args_to_log{
    // clang-format off
    {"Config dir", or_none(cfg.config_dir)},
    {"Hostname", or_none(cfg.hostname)},
    {"Port", or_none(cfg.port)},
    {"Index dir", or_none(cfg.index_dir)},
    {"Methylome dir", or_none(cfg.methylome_dir)},
    {"Metadata dataframe", or_none(cfg.metadata_dataframe)},
    {"Select metadata", or_none(cfg.select_metadata)},
    {"Methylome list", or_none(cfg.methylome_list)},
    {"Log level", to_string(cfg.log_level)},
    {"Genomes", or_none(genomes_joined)},
    {"Download policy", xfr::get_download_policy_message(download_policy)},
    // clang-format on
  };
  xfr::log_args<transferase::log_level_t::info>(args_to_log);

  try {
    cfg.install(genomes, download_policy, empty_sys_config_dir, show_progress);
  }
  catch (const std::exception &e) {
    lgr.error("Error: {}", e.what());
    return EXIT_FAILURE;
  }
  lgr.info("Completed configuration with status: {}",
           error ? error.message() : "Success");

  return EXIT_SUCCESS;
}
