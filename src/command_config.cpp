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
'${HOME}/.config/transferase'. It also will retrieve other data. It
will get index files that are needed to accelerate queries. And it
will retrieve files that associate entries in MethBase2 with labels
for the underlying biological samples.
)";

static constexpr auto examples = R"(
Examples:

xfr config -c my_config_dir -s example.com -p 5009 --genomes hg38,mm39
)";

#include "arguments.hpp"
#include "client_config.hpp"
#include "command_config_argset.hpp"
#include "download.hpp"
#include "find_path_to_binary.hpp"
#include "format_error_code.hpp"  // IWYU pragma: keep
#include "genome_index_data.hpp"
#include "genome_index_metadata.hpp"
#include "logger.hpp"
#include "utilities.hpp"

#include <config.h>  // for VERSION, DATADIR, PROJECT_NAME

#include <boost/describe.hpp>  // for BOOST_DESCRIBE_STRUCT
#include <boost/json.hpp>

#include <cerrno>   // for errno
#include <chrono>   // for std::chrono::operator-
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>  // for std::cbegin
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>  // for std::formatter according to iwyu
#include <tuple>
#include <unordered_map>
#include <utility>  // for std::move
#include <variant>  // IWYU pragma: keep
#include <vector>

namespace transferase {

struct remote_data_resources {
  std::string host;
  std::string port;
  std::string path;

  [[nodiscard]] auto
  form_index_target_stem(const auto &genome) const {
    return (std::filesystem::path{path} / "indexes" / genome).string();
  }

  [[nodiscard]] auto
  form_labels_target_stem(const auto &genome) const {
    return (std::filesystem::path{path} / "labels/latest" / genome).string();
  }

  [[nodiscard]] auto
  form_url(const auto &file) const {
    return std::format("{}:{}{}", host, port, file);
  }
};
BOOST_DESCRIBE_STRUCT(remote_data_resources, (), (host, port, path))

/// Locate the system confuration file with information about servers for data
/// downloads.
[[nodiscard]] static auto
get_remote_data_resources()
  -> std::tuple<std::vector<remote_data_resources>, std::error_code> {
  // ADS: this function prints error messages itself, so no need to
  // log based on error code returned
  static const auto exe_path = find_path_to_binary();
  auto &lgr = transferase::logger::instance();

  std::error_code ec;
  const auto exe_dir_parent =
    std::filesystem::canonical(exe_path).parent_path().parent_path().string();
  bool is_dir = std::filesystem::is_directory(exe_dir_parent, ec);
  if (ec) {
    lgr.error("Error: {} ({})", ec, exe_dir_parent);
    return {{}, ec};
  }
  if (!is_dir) {
    lgr.error("Not a directory: {}", exe_dir_parent);
    return {{}, std::make_error_code(std::errc::not_a_directory)};
  }

  // ADS: DATADIR comes from config.h which comes from config.h.in and
  // is set by cmake
  const auto data_path = std::filesystem::path{DATADIR};
  const auto data_dir = exe_dir_parent / data_path / std::string(PROJECT_NAME);
  is_dir = std::filesystem::is_directory(data_dir, ec);
  if (ec) {
    lgr.error("Error: {} ({})", ec, data_dir.string());
    return {{}, ec};
  }
  if (!is_dir) {
    lgr.error("Not a directory: {}", data_dir.string());
    return {{}, std::make_error_code(std::errc::not_a_directory)};
  }

  const auto json_file =
    data_dir / std::format("{}_data_{}.json", PROJECT_NAME, VERSION);

  std::ifstream in(json_file);
  if (!in)
    return {{}, std::make_error_code(std::errc(errno))};

  const auto filesize =
    static_cast<std::streamsize>(std::filesystem::file_size(json_file, ec));
  if (ec) {
    lgr.error("Bad system config file: {}", json_file.string());
    return {{}, ec};
  }

  std::string payload(filesize, '\0');
  if (!in.read(payload.data(), filesize)) {
    lgr.error("Failure reading file: {}", json_file.string());
    return {{}, std::make_error_code(std::errc(errno))};
  }

  std::vector<remote_data_resources> resources;
  boost::json::parse_into(resources, payload, ec);
  if (ec) {
    lgr.error("Malformed JSON for remote resources {}: {}", json_file.string(),
              ec);
    return {{}, ec};
  }
  return std::make_tuple(std::move(resources), std::error_code{});
}

[[nodiscard]] static auto
dl_err(const auto &hdr, const auto &ec, const auto &url) -> std::error_code {
  auto &lgr = transferase::logger::instance();
  lgr.error("Error downloading {}: ", url);
  if (ec)
    lgr.error("Error code: {}", ec);
  const auto status_itr = hdr.find("Status");
  const auto reason_itr = hdr.find("Reason");
  if (status_itr != std::cend(hdr) && reason_itr != std::cend(hdr))
    lgr.error("HTTP status: {} {}", status_itr->second, reason_itr->second);
  return ec;
}

[[nodiscard]] static auto
check_is_outdated(const download_request &dr,
                  const std::filesystem::path &local_file,
                  std::error_code &ec) -> bool {
  ec.clear();
  const auto local_file_exists = std::filesystem::exists(local_file, ec);
  if (ec)
    return false;
  if (!local_file_exists)
    return true;

  const std::filesystem::file_time_type ftime =
    std::filesystem::last_write_time(local_file, ec);
  if (ec)
    return false;
  const auto remote_timestamp = get_timestamp(dr);
  return (remote_timestamp - ftime).count() > 0;
}

[[nodiscard]] static auto
get_index_files(const remote_data_resources &remote,
                const std::vector<std::string> &genomes,
                const std::string &dirname,
                const bool force_download) -> std::error_code {
  auto &lgr = transferase::logger::instance();
  for (const auto &genome : genomes) {
    const auto stem = remote.form_index_target_stem(genome);

    const auto data_file =
      std::format("{}{}", stem, genome_index_data::filename_extension);

    const download_request dr{remote.host, remote.port, data_file, dirname};
    const auto local_index_file =
      std::filesystem::path{dirname} / std::format("{}.cpg_idx", genome);

    std::error_code ec;
    const bool is_outdated = check_is_outdated(dr, local_index_file, ec);
    if (ec)
      return ec;

    if (is_outdated || force_download) {
      lgr.info("Download: {} to \"{}\"", remote.form_url(data_file), dirname);

      const auto [data_hdr, data_err] =
        download({remote.host, remote.port, data_file, dirname});
      if (data_err)
        return dl_err(data_hdr, data_err, remote.form_url(data_file));

      const auto meta_file =
        std::format("{}{}", stem, genome_index_metadata::filename_extension);
      lgr.info("Download: {} to \"{}\"", remote.form_url(meta_file), dirname);
      const auto [meta_hdr, meta_err] = download({
        remote.host,
        remote.port,
        meta_file,
        dirname,
      });
      if (meta_err)
        return dl_err(meta_hdr, meta_err, remote.form_url(meta_file));
    }
    else if (!force_download)
      lgr.info("Existing index is most recent: {}", local_index_file.string());
  }
  return {};
}

[[nodiscard]] static auto
get_labels_files(const remote_data_resources &remote,
                 const std::vector<std::string> &genomes,
                 const std::string &dirname) -> std::error_code {
  auto &lgr = transferase::logger::instance();
  for (const auto &genome : genomes) {
    const auto stem = remote.form_labels_target_stem(genome);
    const auto labels_file = std::format("{}.json", stem);
    lgr.info("Download: {} to {}", remote.form_url(labels_file), dirname);
    const auto [data_hdr, data_err] = download(download_request{
      remote.host,
      remote.port,
      labels_file,
      dirname,
    });
    if (data_err)
      return dl_err(data_hdr, data_err, remote.form_url(labels_file));
  }
  return {};
}

[[nodiscard]] static auto
configure_client(const std::string &config_dir, const std::string &log_file,
                 const transferase::log_level_t log_level,
                 const std::string &index_dir, const std::string &labels_dir,
                 const std::vector<std::string> &genomes,
                 const bool force_download) -> std::error_code {
  auto &lgr = transferase::logger::instance();

  using transferase::client_config;
  client_config config{};
  std::error_code error;

  config.init_config_dir(config_dir, error);
  if (error) {
    lgr.error("Failed to set config dir {}: {}", config_dir, error);
    return error;
  }

  const std::string config_file_empty{};
  config.init_config_file(config_file_empty, error);
  if (error) {
    lgr.error("Failed to set config file: {}", error);
    return error;
  }
  config.log_level = log_level;
  config.init_log_file(log_file, error);
  if (error) {
    lgr.error("Failed to set log file: {}", error);
    return error;
  }

  // Setup the indexes dir and the labels dir whether or not the user
  // has specified genomes to configure. This must be done prior to
  // writing the config file or attempting to download any config
  // data.
  config.init_index_dir(index_dir, error);
  if (error) {
    lgr.error("Failed to set index directory {}: {}", config.index_dir, error);
    return error;
  }
  config.init_labels_dir(labels_dir, error);
  if (error) {
    lgr.error("Failed to set labels directory {}: {}", config.labels_dir,
              error);
    return error;
  }

  lgr.info("Making config directories");
  error = config.make_directories();
  if (error) {
    lgr.error("Failure creating directories: {}", error);
    return error;
  }

  lgr.info("Writing configuration file");
  error = config.write();
  if (error) {
    lgr.error("Error writing config file: {}", error);
    return error;
  }

  if (!genomes.empty()) {
    // Only attempt downloads if the user specifies one or more
    // genomes. And if the user specifies genomes, but no index_dir
    // or labels_dir, then make the defaults.
    const auto [remotes, remote_err] = transferase::get_remote_data_resources();
    if (remote_err) {
      lgr.error("Error identifying remote server: {}", remote_err);
      return remote_err;
    }

    // Do the downloads, attempting whatever remote resources are
    // available
    for (const auto &remote : remotes) {
      lgr.info("Host for data files: {}:{}", remote.host, remote.port);
      const auto index_err =
        get_index_files(remote, genomes, config.index_dir, force_download);
      if (index_err)
        lgr.error("Error obtaining cpg index files: {}", index_err);

      const auto labels_err =
        get_labels_files(remote, genomes, config.labels_dir);
      if (labels_err)
        lgr.error("Error obtaining labels files: {}", labels_err);

      // ADS: this is broken -- there is no proper check for whether a
      // subsequent server should be attempted.
      if (!index_err && !labels_err)
        break;  // quit if we got what we want
    }
  }

  return std::error_code{};
}

}  // namespace transferase

template <>
struct std::formatter<transferase::remote_data_resources>
  : std::formatter<std::string> {
  auto
  format(const transferase::remote_data_resources &r,
         std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}:{}{}", r.host, r.port, r.path);
  }
};

auto
command_config_main(int argc,
                    char *argv[])  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  -> int {
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
                                    args.quiet ? xfr::log_level_t::error
                                               : xfr::log_level_t::info);
  if (!lgr) {
    std::println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  args.log_options();

  const auto genomes = split_comma(args.genomes);

  // clang-format off
  const auto error = configure_client(
    args.config_dir,
    args.log_file,
    args.log_level,
    args.index_dir,
    args.labels_dir,
    genomes,
    args.force_download);
  // clang-format on
  if (error)
    return EXIT_FAILURE;
  return EXIT_SUCCESS;
}
