/* MIT License
 *
 * Copyright (c) 2025 Andrew D Smith
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

#include "client_config.hpp"
#include "config_file_utils.hpp"
#include "download.hpp"
#include "genome_index_data.hpp"
#include "genome_index_metadata.hpp"
#include "remote_data_resource.hpp"
#include "utilities.hpp"  // for clean_path

#include <boost/json.hpp>

#include <cerrno>
#include <chrono>  // for std::chrono::operator-
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>  // for std::size
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>  // for std::__detail::operator==
#include <vector>

[[nodiscard]] static inline auto
check_and_return_directory(const std::string &dirname,
                           std::error_code &error) -> std::string {
  const bool exists = std::filesystem::exists(dirname, error);
  if (error)
    return {};
  if (!exists)
    return dirname;
  const auto is_dir = std::filesystem::is_directory(dirname, error);
  if (error)
    return {};
  if (!is_dir) {
    error = std::make_error_code(std::errc::not_a_directory);
    return {};
  }
  return dirname;
}

[[nodiscard]] static inline auto
check_and_return_directory(const std::string &left, const std::string &right,
                           std::error_code &error) -> std::string {
  const auto dirname = std::filesystem::path{left} / right;
  return check_and_return_directory(dirname, error);
}

[[nodiscard]] static inline auto
create_dirs_if_needed(const std::string &dirname,
                      std::error_code &error) -> bool {
  // get the status or error if enoent
  const auto status = std::filesystem::status(dirname, error);
  if (error == std::errc::no_such_file_or_directory) {
    // If the file does not exist already, make the directories
    const bool dirs_ok = std::filesystem::create_directories(dirname, error);
    if (error)
      return false;
    return dirs_ok;
  }
  // real error...
  if (error)
    return false;

  // file exists, check if it's a dir
  return std::filesystem::is_directory(status);
}

[[nodiscard]] static inline auto
check_and_return_file(const std::string &filename,
                      std::error_code &error) noexcept -> std::string {
  const auto status = std::filesystem::status(filename, error);
  if (error == std::errc::no_such_file_or_directory) {
    error.clear();
    return filename;
  }
  if (error)
    return {};

  const auto is_dir = std::filesystem::is_directory(status);
  // File exists as a dir
  if (is_dir) {
    error = std::make_error_code(std::errc::is_a_directory);
    return {};
  }

  // File exists but not as dir
  return filename;
}

[[nodiscard]] static inline auto
check_and_return_file(const std::string &left, const std::string &right,
                      std::error_code &error) -> std::string {
  const auto joined = std::filesystem::path{left} / right;
  return check_and_return_file(joined, error);
}

namespace transferase {

[[nodiscard]] auto
client_config::get_config_dir_default(std::error_code &ec) -> std::string {
  auto env_home = std::getenv("HOME");
  if (!env_home) {
    ec = std::make_error_code(std::errc::not_a_directory);
    return {};
  }
  auto env_home_path = std::filesystem::absolute(env_home, ec);
  if (ec)
    return {};
  env_home_path = std::filesystem::weakly_canonical(env_home_path, ec);
  if (ec)
    return {};
  return check_and_return_directory(env_home_path, transferase_config_dirname,
                                    ec);
}

[[nodiscard]] auto
client_config::get_dir_default_impl(const std::string &dirname,
                                    std::error_code &ec) -> std::string {
  const auto config_dir_local =
    config_dir.empty() ? get_config_dir_default(ec) : config_dir;
  if (ec)
    return {};
  return check_and_return_directory(config_dir_local, dirname, ec);
}

[[nodiscard]] auto
client_config::get_index_dir_default(std::error_code &error) -> std::string {
  // ADS: need to replace this with functions that take actual config
  // dir, since the config file in that dir might point to a different
  // location for the index dir.
  const auto config_dir = get_config_dir_default(error);
  if (error)
    return {};
  return check_and_return_directory(config_dir, index_dirname, error);
}

[[nodiscard]] auto
client_config::get_file_default_impl(const std::string &filename,
                                     std::error_code &ec) -> std::string {
  const auto config_dir_local =
    config_dir.empty() ? get_config_dir_default(ec) : config_dir;
  if (ec)
    return {};
  return check_and_return_file(config_dir_local, filename, ec);
}

[[nodiscard]] auto
client_config::get_config_file_default(std::error_code &error) -> std::string {
  const auto config_dir = get_config_dir_default(error);
  if (error)
    return {};
  return check_and_return_file(config_dir, client_config_filename, error);
}

[[nodiscard]] auto
client_config::set_defaults(const bool force) -> std::error_code {
  std::error_code ec;
  if (force || config_file.empty()) {
    config_file = get_file_default_impl(client_config_filename, ec);
    if (ec)
      return ec;
  }

  if (force || index_dir.empty()) {
    index_dir = get_dir_default_impl(index_dirname, ec);
    if (ec)
      return ec;
  }

  if (force || labels_dir.empty()) {
    labels_dir = get_dir_default_impl(labels_dirname, ec);
    if (ec)
      return ec;
  }
  return std::error_code{};
}

// Init routines for the  files / directories
auto
client_config::init_config_dir(const std::string &s,
                               std::error_code &error) -> void {
  auto tmp = s.empty() ? get_config_dir_default(error) : clean_path(s, error);
  if (error)
    return;
  tmp = check_and_return_directory(tmp, error);
  if (error)
    return;
  config_dir = tmp;
}

auto
client_config::init_log_file(const std::string &s,
                             std::error_code &error) -> void {
  log_file = s.empty() ? get_file_default_impl(client_log_filename, error)
                       : clean_path(s, error);
}

auto
client_config::init_config_file(const std::string &s,
                                std::error_code &error) -> void {
  config_file = s.empty() ? get_file_default_impl(client_config_filename, error)
                          : clean_path(s, error);
}

auto
client_config::init_index_dir(const std::string &s,
                              std::error_code &error) -> void {
  index_dir = s.empty() ? get_dir_default_impl(index_dirname, error) : s;
  if (error)
    index_dir.clear();
}

auto
client_config::init_labels_dir(const std::string &s,
                               std::error_code &error) -> void {
  labels_dir = s.empty() ? get_dir_default_impl(labels_dirname, error) : s;
  if (error)
    labels_dir.clear();
}
// done init routines

/// Create all the directories involved in the client config, but if
/// filesystem entires already exist for those directory names,
/// don't do anything. If they happen to already exist but are files
/// and not directories, it will result in an error later.
[[nodiscard]] auto
client_config::make_directories() const -> std::error_code {
  std::error_code ec;

  if (!config_dir.empty()) {
    const bool dirs_ok = create_dirs_if_needed(config_dir, ec);
    if (ec)
      return ec;
    if (!dirs_ok)
      return std::make_error_code(std::errc::not_a_directory);
  }

  if (!config_file.empty()) {
    const auto config_file_clean = clean_path(config_file, ec);
    if (ec)
      return ec;
    const auto config_file_dir =
      std::filesystem::path(config_file_clean).parent_path().string();
    const bool dirs_ok = create_dirs_if_needed(config_file_dir, ec);
    if (ec)
      return ec;
    if (!dirs_ok)
      return std::make_error_code(std::errc::not_a_directory);
  }

  if (!labels_dir.empty()) {
    const auto labels_dir_clean = clean_path(labels_dir, ec);
    if (ec)
      return ec;
    const bool dirs_ok = create_dirs_if_needed(labels_dir_clean, ec);
    if (ec)
      return ec;
    if (!dirs_ok)
      return std::make_error_code(std::errc::not_a_directory);
  }

  if (!index_dir.empty()) {
    const auto index_dir_clean = clean_path(index_dir, ec);
    if (ec)
      return ec;
    const bool dirs_ok = create_dirs_if_needed(index_dir_clean, ec);
    if (ec)
      return ec;
    if (!dirs_ok)
      return std::make_error_code(std::errc::not_a_directory);
  }

  if (!log_file.empty()) {
    const auto log_file_clean = clean_path(log_file, ec);
    if (ec)
      return ec;
    const auto log_file_dir =
      std::filesystem::path(log_file_clean).parent_path().string();
    const bool dirs_ok = create_dirs_if_needed(log_file_dir, ec);
    if (ec)
      return ec;
    if (!dirs_ok)
      return std::make_error_code(std::errc::not_a_directory);
  }

  return {};
}

[[nodiscard]] auto
client_config::write() const -> std::error_code {
  std::ofstream out(config_file);
  if (!out)
    return std::make_error_code(std::errc(errno));
  const auto serialized = format_as_config(*this);

  const auto serialized_size =
    static_cast<std::streamsize>(std::size(serialized));

  out.write(serialized.data(), serialized_size);
  if (!out)
    // EC
    return std::make_error_code(std::errc::bad_file_descriptor);
  return std::error_code{};
}

[[nodiscard]] auto
client_config::tostring() const -> std::string {
  std::ostringstream o;
  if (!(o << boost::json::value_from(*this)))
    o.clear();
  return o.str();
}

[[nodiscard]] static auto
dl_err(const auto &hdr, const auto &ec, const auto &url) -> std::error_code {
  auto &lgr = transferase::logger::instance();
  lgr.debug("Error downloading {}: ", url);
  if (ec)
    lgr.debug("Error code: {}", ec);
  const auto status_itr = hdr.find("Status");
  const auto reason_itr = hdr.find("Reason");
  if (status_itr != std::cend(hdr) && reason_itr != std::cend(hdr))
    lgr.debug("HTTP status: {} {}", status_itr->second, reason_itr->second);
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
      lgr.debug("Existing index is most recent: {}", local_index_file.string());
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

/// Verifying that the client config makes sense and is possible must
/// be done before creating directories, writing config files or
/// attempting to download any config data.
[[nodiscard]] auto
client_config::validate(std::error_code &error) noexcept -> bool {
  error.clear();
  // verify fields that are needed
  auto &lgr = transferase::logger::instance();

  init_config_dir(config_dir, error);
  if (error) {
    lgr.error("Failed to set config dir {}: {}", config_dir, error);
    return false;
  }

  // We want the config file to always get its default; the directory
  // is what the user sets
  init_config_file(std::string{}, error);
  if (error) {
    lgr.debug("Failed to set config file: {}", error);
    return false;
  }

  init_log_file(log_file, error);
  if (error) {
    lgr.debug("Failed to set log file: {}", error);
    return false;
  }

  // Setup the indexes dir and the labels dir whether or not the user
  // has specified genomes to configure.
  init_index_dir(index_dir, error);
  if (error) {
    lgr.debug("Failed to set index directory {}: {}", index_dir, error);
    return false;
  }
  init_labels_dir(labels_dir, error);
  if (error) {
    lgr.debug("Failed to set labels directory {}: {}", labels_dir, error);
    return false;
  }

  return true;
}

auto
client_config::run(const std::vector<std::string> &genomes,
                   const bool force_download,
                   std::error_code &error) const noexcept -> void {
  // validate fields that are needed
  auto &lgr = transferase::logger::instance();

  lgr.debug("Making config directories");
  error = make_directories();
  if (error) {
    lgr.error("Error creating directories: {}", error);
    return;
  }

  lgr.debug("Writing configuration file");
  error = write();
  if (error) {
    lgr.error("Error writing config file: {}", error);
    return;
  }

  if (!genomes.empty()) {
    // Only attempt downloads if the user specifies one or more
    // genomes. And if the user specifies genomes, but no index_dir
    // or labels_dir, then make the defaults.
    const auto remotes = transferase::get_remote_data_resources(error);
    if (error) {
      lgr.error("Error identifying remote server: {}", error);
      return;
    }

    // Do the downloads, attempting whatever remote resources are
    // available
    bool all_downloads_ok{};
    for (const auto &remote : remotes) {
      lgr.debug("Attempting download from {}:{}", remote.host, remote.port);
      const auto index_err =
        get_index_files(remote, genomes, index_dir, force_download);
      if (index_err)
        lgr.debug("Error obtaining index files: {}", index_err);
      const auto labels_err = get_labels_files(remote, genomes, labels_dir);
      if (labels_err)
        lgr.debug("Error obtaining labels files: {}", labels_err);

      // ADS: this is broken -- there is no proper check for whether a
      // subsequent server should be attempted.
      all_downloads_ok = (!index_err && !labels_err);
      if (all_downloads_ok)
        break;  // quit if we got what we want
    }
    if (!all_downloads_ok)
      error = std::make_error_code(std::errc::invalid_argument);
  }
  error.clear();  // ok!
}

auto
client_config::run(const std::vector<std::string> &genomes,
                   const std::string &system_config_dir,
                   const bool force_download,
                   std::error_code &error) const noexcept -> void {
  // validate fields that are needed
  auto &lgr = transferase::logger::instance();

  lgr.debug("Making config directories");
  error = make_directories();
  if (error) {
    error = client_config_error_code::error_creating_directories;
    lgr.error("Error creating directories: {}", error);
    return;
  }

  lgr.debug("Writing configuration file");
  error = write();
  if (error) {
    error = client_config_error_code::error_writing_config_file;
    lgr.error("Error writing config file: {}", error);
    return;
  }

  if (!genomes.empty()) {
    // Only attempt downloads if the user specifies one or more
    // genomes. And if the user specifies genomes, but no index_dir
    // or labels_dir, then make the defaults.
    const auto remotes =
      transferase::get_remote_data_resources(system_config_dir, error);
    if (error) {
      error = client_config_error_code::error_identifying_remote_resources;
      lgr.error("Error identifying remote server: {}", error);
      return;
    }

    // Do the downloads, attempting whatever remote resources are
    // available
    bool all_downloads_ok{};
    for (const auto &remote : remotes) {
      lgr.debug("Attempting download from {}:{}", remote.host, remote.port);
      const auto index_err =
        get_index_files(remote, genomes, index_dir, force_download);
      if (index_err)
        lgr.debug("Error obtaining index files: {}", index_err);
      const auto labels_err = get_labels_files(remote, genomes, labels_dir);
      if (labels_err)
        lgr.debug("Error obtaining labels files: {}", labels_err);

      // ADS: this is broken -- there is no proper check for whether a
      // subsequent server should be attempted.
      all_downloads_ok = (!index_err && !labels_err);
      if (all_downloads_ok)
        break;  // quit if we got what we want
    }
    if (!all_downloads_ok)
      error = client_config_error_code::download_error;
  }
  error.clear();  // ok!
}

}  // namespace transferase
