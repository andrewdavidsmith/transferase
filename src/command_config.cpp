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
configure an xfrase client
)";

static constexpr auto description = R"(
This command does the configuration to faciliate other commands,
reducing the number of command line arguments by putting them in
configuration file. It also will retrieve index files that are needed
to accelerate queries. Note that this configuration is not needed, as
all arguments can be specified on the command line and index files can
be downloaded separately. The default config directory is
'${HOME}/.config/transferase'.
)";

static constexpr auto examples = R"(
Examples:

xfrase config -c my_config_file.toml -s example.com -p 5009 --assemblies hg38,mm39
)";

#include "arguments.hpp"
#include "command_config_argset.hpp"
#include "config_file_utils.hpp"  // write_client_config_file
#include "cpg_index_data.hpp"
#include "cpg_index_metadata.hpp"
#include "download.hpp"
#include "find_path_to_binary.hpp"
#include "utilities.hpp"
#include "xfrase_error.hpp"  // IWYU pragma: keep

#include <config.h>  // for VERSION, DATADIR, PROJECT_NAME

#include <boost/describe.hpp>  // for BOOST_DESCRIBE_STRUCT
#include <boost/json.hpp>

#include <algorithm>
#include <cerrno>   // for errno
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
#include <thread>  // for std::formatter according to iwyu
#include <tuple>
#include <unordered_map>
#include <utility>  // for std::move
#include <variant>  // IWYU pragma: keep
#include <vector>

template <>
struct std::formatter<std::filesystem::path> : std::formatter<std::string> {
  auto
  format(const std::filesystem::path &p, std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}", p.string());
  }
};

namespace transferase {

struct remote_indexes_resources {
  std::string host;
  std::string port;
  std::string path;

  [[nodiscard]]
  auto
  form_target_stem(const auto &assembly) const {
    return (std::filesystem::path{path} / assembly).string();
  }
  [[nodiscard]]
  auto
  form_url(const auto &file) const {
    return std::format("{}:{}{}", host, port, file);
  }
};
BOOST_DESCRIBE_STRUCT(remote_indexes_resources, (), (host, port, path))

[[nodiscard]] static auto
get_remote_indexes_resources()
  -> std::tuple<std::vector<remote_indexes_resources>, std::error_code> {
  // ADS: this function prints error messages itself

  static const auto exe_path = find_path_to_binary();

  std::error_code ec;
  const auto exe_dir_parent =
    std::filesystem::canonical(exe_path).parent_path().parent_path();
  bool is_dir = std::filesystem::is_directory(exe_dir_parent, ec);
  if (ec) {
    std::println("Error: {} ({})", ec, exe_dir_parent);
    return {{}, ec};
  }
  if (!is_dir) {
    std::println("Not a directory: {}", exe_dir_parent);
    return {{}, std::make_error_code(std::errc::not_a_directory)};
  }

  // ADS: DATADIR comes from config.h which comes from config.h.in and
  // is set by cmake
  const auto data_path = std::filesystem::path{DATADIR};
  const auto data_dir = exe_dir_parent / data_path / std::string(PROJECT_NAME);
  is_dir = std::filesystem::is_directory(data_dir, ec);
  if (ec) {
    std::println("Error: {} ({})", ec, data_dir);
    return {{}, ec};
  }
  if (!is_dir) {
    std::println("Not a directory: {}", data_dir);
    return {{}, std::make_error_code(std::errc::not_a_directory)};
  }

  const auto json_file =
    data_dir / std::format("{}_data_{}.json", PROJECT_NAME, VERSION);

  std::ifstream in(json_file);
  if (!in)
    return {{}, std::make_error_code(std::errc(errno))};

  const auto filesize = std::filesystem::file_size(json_file, ec);
  if (ec) {
    std::println("Bad system config file: {}", json_file);
    return {{}, ec};
  }

  std::string payload(filesize, '\0');
  if (!in.read(payload.data(), filesize)) {
    std::println("Failure reading file: {}", json_file);
    return {{}, std::make_error_code(std::errc(errno))};
  }

  std::vector<remote_indexes_resources> resources;
  boost::json::parse_into(resources, payload, ec);
  if (ec) {
    std::println("Malformed JSON for remote resources {}: {}", json_file, ec);
    return {{}, ec};
  }
  return std::make_tuple(std::move(resources), std::error_code{});
}

[[nodiscard]]
static auto
get_index_files(const bool quiet, const remote_indexes_resources &remote,
                const std::string &assemblies,
                const std::string &dirname) -> std::error_code {
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
    const auto stem = remote.form_target_stem(assem);
    const auto data_file = std::format(
      "{}{}", stem, transferase::cpg_index_data::filename_extension);
    if (!quiet)
      std::println("Download: {}", remote.form_url(data_file));
    const auto [data_hdr, data_err] =
      download({remote.host, remote.port, data_file, dirname});
    if (data_err)
      dl_err(data_hdr, data_err, remote.form_url(data_file));
    const auto meta_file = std::format(
      "{}{}", stem, transferase::cpg_index_metadata::filename_extension);
    if (!quiet)
      std::println("Download: {}", remote.form_url(meta_file));
    const auto [meta_hdr, meta_err] =
      download({remote.host, remote.port, meta_file, dirname});
    if (meta_err)
      dl_err(meta_hdr, meta_err, remote.form_url(meta_file));
  }
  return {};
}

}  // namespace transferase

template <>
struct std::formatter<transferase::remote_indexes_resources>
  : std::formatter<std::string> {
  auto
  format(const transferase::remote_indexes_resources &r,
         std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}:{}{}", r.host, r.port, r.path);
  }
};

auto
command_config_main(int argc, char *argv[]) -> int {
  static constexpr auto command = "config";
  static const auto usage =
    std::format("Usage: xfrase {} [options]\n", strip(command));
  static const auto about_msg =
    std::format("xfrase {}: {}", strip(command), strip(about));
  static const auto description_msg =
    std::format("{}\n{}", strip(description), strip(examples));

  transferase::command_config_argset args;
  auto ec = args.parse(argc, argv, usage, about_msg, description_msg);
  if (ec == argument_error::help_requested)
    return EXIT_SUCCESS;
  if (ec)
    return EXIT_FAILURE;

  std::shared_ptr<std::ostream> log_file =
    args.log_filename.empty()
      ? std::make_shared<std::ostream>(std::cout.rdbuf())
      : std::make_shared<std::ofstream>(args.log_filename, std::ios::app);

  if (!args.quiet)
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
  if (!args.quiet)
    std::println("Client config directory: {}", config_dir);

  {
    // make sure the user specified config dir exists
    const bool dir_exists = std::filesystem::exists(config_dir, ec);
    if (dir_exists && !std::filesystem::is_directory(config_dir, ec)) {
      std::println("File exists and is not a directory: {}", config_dir);
      return EXIT_FAILURE;
    }
    if (!dir_exists) {
      if (!args.quiet)
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

  const auto [remotes, remote_err] =
    transferase::get_remote_indexes_resources();
  if (remote_err) {
    std::println("Error identifying remote server: {}", remote_err);
    return EXIT_FAILURE;
  }

  // take care of obtaining index files
  for (const auto &remote : remotes) {
    if (!args.quiet)
      std::println("Host for index files: {}:{}", remote.host, remote.port);
    const auto index_err =
      get_index_files(args.quiet, remote, args.assemblies, config_dir);
    if (index_err) {
      std::println("Error obtaining cpg index files: {}", index_err);
      return EXIT_FAILURE;
    }
    break;  // quit if we got what we want
  }
  return EXIT_SUCCESS;
}
