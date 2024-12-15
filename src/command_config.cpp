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

mxe config -c my_config_file.toml -s example.com -p 5009 --assemblies hg38,mm39
)";

#include "arguments.hpp"
#include "command_config_argset.hpp"
#include "config_file_utils.hpp"  // write_client_config_file
#include "download.hpp"
#include "mxe_error.hpp"  // IWYU pragma: keep
#include "utilities.hpp"

#include <algorithm>
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>  // for std::cbegin
#include <memory>    // std::make_shared
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <variant>  // IWYU pragma: keep
#include <vector>

[[nodiscard]]
static auto
form_target_stem(const auto &assembly) {
  static constexpr auto base_url = "/lab/public/transferase/indexes";
  return (std::filesystem::path{base_url} / assembly).string();
}

[[nodiscard]]
static auto
form_url(const auto &host, const auto &port, const auto &file) {
  return std::format("{}:{}{}", host, port, file);
}

[[nodiscard]]
static auto
get_index_files(const std::string &assemblies,
                const std::string &dirname) -> std::error_code {
  static constexpr auto cpg_index_data_suff = ".cpg_idx";
  static constexpr auto cpg_index_meta_suff = ".cpg_idx.json";
  static constexpr auto host = "smithlab.usc.edu";
  static constexpr auto port = "80";

  const auto dl_err = [&](const auto &hdr, const auto &ec, const auto &url) {
    std::println("Error downloading {}: ", url);
    if (ec)
      std::println("Error code: {}", ec);
    const auto status_itr = hdr.find("Status");
    const auto reason_itr = hdr.find("Reason");
    if (status_itr != std::cend(hdr) && reason_itr != std::cend(hdr))
      std::println("HTTP status: {} {}", status_itr->second,
                   reason_itr->second);
  };

  for (const auto assembly : std::views::split(assemblies, ',')) {
    const auto assem = std::string{std::cbegin(assembly), std::cend(assembly)};
    const auto stem = form_target_stem(assem);
    const auto data_file = std::format("{}{}", stem, cpg_index_data_suff);
    const auto meta_file = std::format("{}{}", stem, cpg_index_meta_suff);
    const auto [data_hdr, data_err] = download(host, port, data_file, dirname);
    if (data_err)
      dl_err(data_hdr, data_err, form_url(host, port, data_file));
    const auto [meta_hdr, meta_err] = download(host, port, meta_file, dirname);
    if (meta_err)
      dl_err(meta_hdr, meta_err, form_url(host, port, meta_file));
  }
  return {};
}

auto
command_config_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "config";
  static const auto usage =
    std::format("Usage: mxe {} [options]\n", strip(command));
  static const auto about_msg =
    std::format("mxe {}: {}", strip(command), strip(about));
  static const auto description_msg =
    std::format("{}\n{}", strip(description), strip(examples));

  command_config_argset args;
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
    // make sure the user specified config dir exists
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

  const auto config_write_err = write_client_config_file(args);
  if (config_write_err) {
    std::println("Error writing config file: {}", config_write_err);
    return EXIT_FAILURE;
  }

  // take care of obtaining index files
  const auto index_err = get_index_files(args.assemblies, config_dir);
  if (index_err) {
    std::println("Exit with error obtaining cpg index files: {}", index_err);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
