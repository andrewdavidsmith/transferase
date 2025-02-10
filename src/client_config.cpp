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
#include "find_path_to_binary.hpp"
#include "genome_index_data.hpp"
#include "genome_index_metadata.hpp"
#include "remote_data_resource.hpp"
#include "utilities.hpp"  // for clean_path

#include <config.h>

#include <boost/json.hpp>
#include <boost/mp11/algorithm.hpp>

#include <cassert>
#include <chrono>  // for std::chrono::operator-
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iterator>  // for std::size
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
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

/// Locate the system confuration directory based on the path to the binary for
/// the currently running process.
[[nodiscard]] auto
get_default_sys_config_dir(std::error_code &error) noexcept -> std::string {
  namespace fs = std::filesystem;
  auto &lgr = logger::instance();
  static const auto exe_path = find_path_to_binary();
  const auto exe_dir_parent =
    fs::canonical(exe_path).parent_path().parent_path().string();
  const bool is_dir = fs::is_directory(exe_dir_parent, error);
  if (error) {
    lgr.debug("Error: {} ({})", error, exe_dir_parent);
    return {};
  }
  if (!is_dir) {
    error = std::make_error_code(std::errc::not_a_directory);
    lgr.debug("Not a directory: {}", exe_dir_parent);
    return {};
  }
  // ADS: DATADIR comes from config.h which comes from config.h.in and
  // is set by cmake
  return exe_dir_parent / fs::path{DATADIR} / std::string(PROJECT_NAME);
}

[[nodiscard]] auto
client_config::get_default_config_dir(std::error_code &ec) -> std::string {
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
  return check_and_return_directory(env_home_path,
                                    transferase_config_dirname_default, ec);
}

[[nodiscard]] auto
client_config::get_default_index_dir(std::error_code &error) -> std::string {
  // ADS: need to replace this with functions that take actual config
  // dir, since the config file in that dir might point to a different
  // location for the index dir.
  const auto config_dir = get_default_config_dir(error);
  if (error)
    return {};
  return check_and_return_directory(config_dir, index_dirname_default, error);
}

[[nodiscard]] inline auto
get_dir_default_impl(const std::string &dirname,
                     std::error_code &error) -> std::string {
  const auto config_dir_local = client_config::get_default_config_dir(error);
  if (error)
    return {};
  return check_and_return_directory(config_dir_local, dirname, error);
}

[[nodiscard]] inline auto
get_file_default_impl(const std::string &filename,
                      std::error_code &error) -> std::string {
  const auto config_dir_local = client_config::get_default_config_dir(error);
  if (error)
    return {};
  return check_and_return_file(config_dir_local, filename, error);
}

[[nodiscard]] auto
client_config::get_config_file(const std::string &config_dir,
                               std::error_code &error) -> std::string {
  return check_and_return_file(config_dir, client_config_filename_default,
                               error);
}

auto
client_config::set_defaults(std::string config_dir,
                            const std::string &sys_config_dir,
                            std::error_code &error) noexcept -> void {
  namespace fs = std::filesystem;
  if (config_dir.empty())
    config_dir = get_default_config_dir(error);

  const auto [default_hostname, default_port] =
    transferase::get_transferase_server_info(sys_config_dir, error);
  if (error)
    return;

  hostname = default_hostname;
  port = default_port;

  const auto dirname = (fs::path{config_dir} / index_dirname_default).string();
  index_dir = get_dir_default_impl(dirname, error);
  if (error)
    return;

  const auto filename =
    (fs::path{config_dir} / metadata_filename_default).string();
  metadata_file = get_file_default_impl(filename, error);
  if (error)
    return;
}

auto
client_config::set_defaults(std::string config_dir,
                            std::error_code &error) noexcept -> void {
  if (config_dir.empty()) {
    config_dir = get_default_config_dir(error);
    if (error)
      return;
  }
  const std::string sys_config_dir = get_default_sys_config_dir(error);
  if (error)
    return;
  set_defaults(config_dir, sys_config_dir, error);
}

/// Create all the directories involved in the client config, if they
/// do not already exist. If directories exist as files, set the error
/// code.
auto
client_config::make_directories(const std::string &config_dir_arg,
                                std::error_code &error) const noexcept -> void {
  namespace fs = std::filesystem;
  assert(!config_dir_arg.empty());

  const std::string config_dir = fs::weakly_canonical(config_dir_arg, error);
  if (error)
    return;

  assert(!config_dir.empty());

  const bool dirs_ok = create_dirs_if_needed(config_dir, error);
  if (error)
    return;
  if (!dirs_ok) {
    error = std::make_error_code(std::errc::not_a_directory);
    return;
  }

  const auto config_file = get_config_file(config_dir, error);
  if (error)
    return;

  assert(!config_file.empty());

  const auto config_file_clean = clean_path(config_file, error);
  if (error)
    return;

  if (!metadata_file.empty()) {
    const auto metadata_file_clean = clean_path(metadata_file, error);
    if (error)
      return;
    const auto metadata_file_dir =
      fs::path(metadata_file_clean).parent_path().string();
    const bool dirs_ok = create_dirs_if_needed(metadata_file_dir, error);
    if (error)
      return;
    if (!dirs_ok) {
      error = std::make_error_code(std::errc::not_a_directory);
      return;
    }
  }

  if (!index_dir.empty()) {
    const auto index_dir_clean = clean_path(index_dir, error);
    if (error)
      return;
    const bool dirs_ok = create_dirs_if_needed(index_dir_clean, error);
    if (error)
      return;
    if (!dirs_ok) {
      error = std::make_error_code(std::errc::not_a_directory);
      return;
    }
  }

  if (!log_file.empty()) {
    const auto log_file_clean = clean_path(log_file, error);
    if (error)
      return;
    const auto log_file_dir = fs::path(log_file_clean).parent_path().string();
    const bool dirs_ok = create_dirs_if_needed(log_file_dir, error);
    if (error)
      return;
    if (!dirs_ok) {
      error = std::make_error_code(std::errc::not_a_directory);
      return;
    }
  }
}

[[nodiscard]] auto
client_config::read(std::string config_dir,
                    std::error_code &error) noexcept -> client_config {
  // If config dir is empty, get the default
  if (config_dir.empty()) {
    config_dir = get_default_config_dir(error);
    if (error)
      return {};
  }
  // Get the config filename
  const auto config_file = get_config_file(config_dir, error);
  if (error)
    return {};
  client_config config;
  parse_config_file(config, config_file, error);
  if (error)
    return {};
  return config;
}

auto
client_config::save(std::string config_dir,
                    std::error_code &error) const noexcept -> void {
  if (config_dir.empty()) {
    config_dir = get_default_config_dir(error);
    if (error)
      return;
  }
  const auto config_file = get_config_file(config_dir, error);
  if (error)
    return;

  const bool file_exists = std::filesystem::exists(config_file, error);
  if (error)
    return;

  /// ADS: need to extract this pattern of updates intead of writes.
  client_config tmp = *this;
  if (file_exists) {
    // load current config values into a separate object
    tmp = read(config_dir, error);
    if (error)
      return;
    using Md = boost::describe::describe_members<client_config,
                                                 boost::describe::mod_public>;
    // All non-empty string values from the current object
    boost::mp11::mp_for_each<Md>([&](const auto &D) {
      if constexpr (std::is_same_v<decltype(D), std::string>)
        if (!((*this).*D.pointer.empty()))
          tmp.*D.pointer = (*this).*D.pointer;
    });
    // always overwrite log level -- no way to know not to
    tmp.log_level = log_level;
  }
  error = write_config_file(tmp, config_file);
  if (error)
    error = client_config_error_code::error_writing_config_file;
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
                const download_policy_t download_policy) -> std::error_code {
  auto &lgr = transferase::logger::instance();
  for (const auto &genome : genomes) {
    const auto stem = remote.form_index_target_stem(genome);

    const auto data_file =
      std::format("{}{}", stem, genome_index_data::filename_extension);

    const download_request dr{remote.host, remote.port, data_file, dirname};
    const auto local_index_file =
      std::filesystem::path{dirname} / std::format("{}.cpg_idx", genome);

    std::error_code error;
    const bool index_file_exists =
      std::filesystem::exists(local_index_file, error);
    if (error)
      return error;

    const bool is_outdated = index_file_exists &&
                             (download_policy == download_policy_t::update) &&
                             check_is_outdated(dr, local_index_file, error);
    if (error)
      return error;

    // ADS: should report the reason why a download happened
    if (download_policy == download_policy_t::all ||
        (download_policy == download_policy_t::missing && !index_file_exists) ||
        (download_policy == download_policy_t::update && is_outdated)) {
      lgr.debug(R"(Download: {} to "{}")", remote.form_url(data_file), dirname);
      lgr.debug("Reason: policy={}, file_exists={}, is_outdated={}",
                download_policy, index_file_exists, is_outdated);

      const auto [data_hdr, data_err] =
        download({remote.host, remote.port, data_file, dirname});
      if (data_err)
        return dl_err(data_hdr, data_err, remote.form_url(data_file));

      const auto meta_file =
        std::format("{}{}", stem, genome_index_metadata::filename_extension);
      lgr.debug(R"(Download: {} to "{}")", remote.form_url(meta_file), dirname);
      const auto [meta_hdr, meta_err] =
        download({remote.host, remote.port, meta_file, dirname});
      if (meta_err)
        return dl_err(meta_hdr, meta_err, remote.form_url(meta_file));
    }
  }
  return {};
}

[[nodiscard]] static auto
get_metadata_file(const remote_data_resources &remote,
                  const std::string &dirname,
                  const download_policy_t download_policy) -> std::error_code {
  const auto stem = remote.get_metadata_target_stem();
  const auto metadata_file = std::format("{}.json", stem);
  const auto local_metadata_file =
    std::filesystem::path{dirname} / client_config::metadata_filename_default;
  auto &lgr = transferase::logger::instance();

  std::error_code error;
  const bool metadata_file_exists =
    std::filesystem::exists(local_metadata_file, error);
  if (error)
    return error;

  // ADS: why do this check for 'is_outdated' for such a small file?
  // Maybe local modifications are only worth overwriting if the
  // remote is newer?
  const download_request dr{
    remote.host,
    remote.port,
    metadata_file,
    dirname,
  };

  const bool is_outdated = metadata_file_exists &&
                           (download_policy == download_policy_t::update) &&
                           check_is_outdated(dr, local_metadata_file, error);
  if (error)
    return error;

  if (download_policy == download_policy_t::all ||
      (download_policy == download_policy_t::missing &&
       !metadata_file_exists) ||
      (download_policy == download_policy_t::update && is_outdated)) {
    lgr.debug("Download: {} to {}", remote.form_url(metadata_file), dirname);
    lgr.debug("Reason: policy={}, file_exists={}, is_outdated={}",
              download_policy, metadata_file_exists, is_outdated);

    const auto [data_hdr, data_err] = download(dr);
    if (data_err)
      return dl_err(data_hdr, data_err, remote.form_url(metadata_file));
  }
  return {};
}

auto
client_config::configure(const std::vector<std::string> &genomes,
                         const download_policy_t download_policy,
                         std::string config_dir, std::string sys_config_dir,
                         std::error_code &error) const noexcept -> void {
  if (sys_config_dir.empty()) {
    sys_config_dir = get_default_sys_config_dir(error);
    if (error)
      return;
  }
  if (config_dir.empty()) {
    config_dir = get_default_config_dir(error);
    if (error)
      return;
  }
  auto &lgr = logger::instance();

  const bool valid = validate(error);
  if (!valid || error)
    return;

  lgr.debug("Making config directories");
  make_directories(config_dir, error);
  if (error) {
    error = client_config_error_code::error_creating_directories;
    lgr.debug("Error creating directories: {}", error);
    return;
  }

  lgr.debug("Writing configuration file");
  save(config_dir, error);
  if (error) {
    lgr.debug("Error writing config file: {}", error);
    return;
  }

  const auto remotes =
    transferase::get_remote_data_resources(sys_config_dir, error);
  if (error) {
    error = client_config_error_code::error_identifying_remote_resources;
    lgr.debug("Error identifying remote server: {}", error);
    return;
  }

  // Do the downloads, attempting each remote resources server in the
  // system config
  bool metadata_downloads_ok{false};
  for (const auto &remote : remotes) {
    const auto metadata_err =
      get_metadata_file(remote, config_dir, download_policy);
    if (metadata_err)
      lgr.debug("Error obtaining metadata file: {}", metadata_err);
    if (!metadata_err) {
      metadata_downloads_ok = true;
      break;
    }
  }
  if (!metadata_downloads_ok) {
    error = client_config_error_code::metadata_download_error;
    return;
  }

  if (genomes.empty())
    return;

  bool genome_downloads_ok{false};
  for (const auto &remote : remotes) {
    const auto index_err =
      get_index_files(remote, genomes, index_dir, download_policy);
    if (index_err)
      lgr.debug("Error obtaining index files: {}", index_err);
    if (!index_err) {
      genome_downloads_ok = true;
      break;
    }
  }
  if (!genome_downloads_ok)
    error = client_config_error_code::genome_index_download_error;

  return;
}

/// Validate that the client config makes sense. This must be done
/// before creating directories, writing config files or attempting to
/// download any config data.
[[nodiscard]] auto
client_config::validate(std::error_code &error) const noexcept -> bool {
  // validate the hostname
  if (hostname.empty()) {
    error = client_config_error_code::invalid_client_config_information;
    return false;
  }
  // validate the port
  if (port.empty()) {
    error = client_config_error_code::invalid_client_config_information;
    return false;
  }
  // validate the index_dir
  if (index_dir.empty()) {
    error = client_config_error_code::invalid_client_config_information;
    return false;
  }
  // validate the metadata_file
  if (metadata_file.empty()) {
    error = client_config_error_code::invalid_client_config_information;
    return false;
  }
  return true;
}

}  // namespace transferase
