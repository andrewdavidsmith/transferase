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
configure an xfr client
)";

static constexpr auto description = R"(
This command does the configuration to faciliate other commands,
reducing the number of command line arguments by putting them in
configuration file. Note that this configuration is not needed, as all
arguments can be specified on the command line and index files can be
downloaded separately. The default config directory is
'${HOME}/.config/transferase'. This command will also retrieve other
data. It will get index files that are used to accelerate queries. And
it will retrieve files that associate entries in MethBase2 with labels
for the underlying biological samples.
)";

static constexpr auto examples = R"(
Examples:

xfr config -c my_config_dir -s example.com -p 5009 --genomes hg38,mm39
)";

#include "arguments.hpp"
#include "client_config.hpp"
#include "command_config_argset.hpp"
#include "logger.hpp"
#include "utilities.hpp"

#include <cstdlib>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <system_error>

auto
command_config_main(int argc, char *argv[]) -> int {  // NOLINT(*-c-arrays)
  static constexpr auto command = "config";
  static const auto usage =
    std::format("Usage: xfr {} [options]\n", rstrip(command));
  static const auto about_msg =
    std::format("xfr {}: {}", rstrip(command), rstrip(about));
  static const auto description_msg =
    std::format("{}\n{}", rstrip(description), rstrip(examples));

  namespace xfr = transferase;

  xfr::command_config_argset args;
  auto ec = args.parse(argc, argv, usage, about_msg, description_msg);
  if (ec == argument_error_code::help_requested)
    return EXIT_SUCCESS;
  if (ec)
    return EXIT_FAILURE;

  auto &lgr = xfr::logger::instance(xfr::shared_from_cout(), command,
                                    args.debug   ? xfr::log_level_t::debug
                                    : args.quiet ? xfr::log_level_t::error
                                                 : xfr::log_level_t::info);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  args.log_options();

  const auto genomes = split_comma(args.genomes);

  xfr::client_config config{
    .config_dir = args.config_dir,
    .config_file = std::string{},  // because we don't set this
    .hostname = args.hostname,
    .port = args.port,
    .index_dir = args.index_dir,
    .labels_dir = args.labels_dir,
    .methylome_dir = args.methylome_dir,
    .log_file = args.log_file,
    .log_level = args.log_level,
  };

  std::error_code error;
  const bool validated = config.validate(error);
  if (!validated || error)
    return EXIT_FAILURE;

  config.run(genomes, args.force_download, error);
  if (error)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}
