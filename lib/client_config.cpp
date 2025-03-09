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
#include "download.hpp"
#include "genome_index_data.hpp"
#include "genome_index_metadata.hpp"
#include "remote_data_resource.hpp"
#include "system_config.hpp"

#include "nlohmann/json.hpp"

#include <cassert>
#include <chrono>  // for std::chrono::operator-
#include <cstdlib>
#include <exception>  // for std::exception
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>  // for std::size
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

[[nodiscard]] static inline auto
get_file_if_not_already_dir(const std::string &filename,
                            std::error_code &error) noexcept -> std::string {
  const auto status = std::filesystem::status(filename, error);
  if (!std::filesystem::exists(status)) {
    error.clear();  // clear enoent
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

namespace transferase {

auto
client_config::make_paths_absolute() noexcept -> void {
  namespace fs = std::filesystem;
  // errors in absolute are for std::bad_alloc
  std::error_code ignored_error;
  if (!index_dir.empty())
    index_dir = fs::absolute(index_dir, ignored_error).string();
  if (!metadata_file.empty())
    metadata_file = fs::absolute(metadata_file, ignored_error).string();
  if (!methylome_dir.empty())
    methylome_dir = fs::absolute(methylome_dir, ignored_error).string();
  if (!log_file.empty())
    log_file = fs::absolute(log_file, ignored_error).string();
  assert(!ignored_error);
}

/// Get the path to the config file (base on dir)
[[nodiscard]] auto
client_config::get_config_file(const std::string &config_dir) noexcept
  -> std::string {
  return (std::filesystem::path(config_dir) / client_config_filename_default)
    .lexically_normal();
}

/// Get the path to the index directory
[[nodiscard]] auto
client_config::get_index_dir() const noexcept -> std::string {
  return (std::filesystem::path(config_dir) / index_dir).lexically_normal();
}

/// Get the path to the metadata file
[[nodiscard]] auto
client_config::get_metadata_file() const noexcept -> std::string {
  return (std::filesystem::path(config_dir) / metadata_file).lexically_normal();
}

/// Get the path to the methylome directory
[[nodiscard]] auto
client_config::get_methylome_dir() const noexcept -> std::string {
  return (std::filesystem::path(config_dir) / methylome_dir).lexically_normal();
}

/// Get the path to the log file
[[nodiscard]] auto
client_config::get_log_file() const noexcept -> std::string {
  return (std::filesystem::path(config_dir) / log_file).lexically_normal();
}

[[nodiscard]] auto
client_config::get_default_config_dir(std::error_code &error) -> std::string {
  auto env_home = std::getenv("HOME");
  if (!env_home) {
    error = std::make_error_code(std::errc::not_a_directory);
    return {};
  }
  auto env_home_path = std::filesystem::absolute(env_home, error);
  if (error)
    return {};
  const auto dirname =
    std::filesystem::path{env_home_path} / transferase_config_dirname_default;
  return dirname.string();
}

[[nodiscard]] auto
client_config::get_config_file(const std::string &config_dir,
                               std::error_code &error) -> std::string {
  const auto joined =
    std::filesystem::path{config_dir} / client_config_filename_default;
  return get_file_if_not_already_dir(joined, error);
}

auto
client_config::assign_defaults_to_missing(std::string sys_config_dir,
                                          std::error_code &error) -> void {
  if (hostname.empty() || port.empty()) {
    if (sys_config_dir.empty()) {
      sys_config_dir = get_default_system_config_dirname();
      if (error)
        return;
    }
    system_config cfg(sys_config_dir);
    if (hostname.empty())
      hostname = cfg.hostname;
    if (port.empty())
      port = cfg.port;
  }
  if (index_dir.empty())
    index_dir = index_dirname_default;
  if (metadata_file.empty())
    metadata_file = metadata_filename_default;
}

client_config::client_config(const std::string &config_dir_arg,
                             const std::string &sys_config_dir,
                             std::error_code &error) :
  config_dir{config_dir_arg} {
  if (config_dir.empty()) {
    config_dir = get_default_config_dir(error);
    if (error)
      throw std::system_error(error, "[Calling get_default_config_dir]");
  }
  const system_config sys_conf(sys_config_dir);
  hostname = sys_conf.hostname;
  port = sys_conf.port;
  index_dir = index_dirname_default;
  metadata_file = metadata_filename_default;
}

client_config::client_config(const std::string &config_dir_arg,
                             std::error_code &error) :
  config_dir{config_dir_arg} {
  if (config_dir.empty()) {
    config_dir = get_default_config_dir(error);
    if (error)
      throw std::system_error(error, "[Calling get_default_config_dir]");
  }
  const std::string sys_conf_dir = get_default_system_config_dirname();
  const system_config sys_conf(sys_conf_dir);
  hostname = sys_conf.hostname;
  port = sys_conf.port;
  index_dir = index_dirname_default;
  metadata_file = metadata_filename_default;
}

client_config::client_config(const std::string &config_dir_arg,
                             const std::string &sys_config_dir) :
  config_dir{config_dir_arg} {
  std::error_code error;
  if (config_dir.empty()) {
    config_dir = get_default_config_dir(error);
    if (error)
      throw std::system_error(error, "[Error in get_default_config_dir]");
  }
  const system_config sys_conf(sys_config_dir);
  hostname = sys_conf.hostname;
  port = sys_conf.port;
  index_dir = index_dirname_default;
  metadata_file = metadata_filename_default;
}

/// Create all the directories involved in the client config, if they
/// do not already exist. If directories exist as files, set the error
/// code.
auto
client_config::make_directories(std::error_code &error) const noexcept -> void {
  namespace fs = std::filesystem;
  assert(!config_dir.empty());

  const auto create = [&error](const auto &dirname) {
    std::filesystem::create_directories(dirname, error);
  };

  create(config_dir);
  if (error)
    return;

  if (!metadata_file.empty()) {
    const auto metadata_file_path = fs::path(metadata_file);
    // If the path isn't absolute, we already have the directory by
    // definition
    if (metadata_file_path.is_absolute()) {
      const auto metadata_file_dir = metadata_file_path.parent_path().string();
      create(metadata_file_dir);
      if (error)
        return;
    }
  }

  if (!index_dir.empty()) {
    // If the path isn't absolute, prepend the config dir
    const auto index_dir_path = fs::path(config_dir) / index_dir;
    create(index_dir_path.string());
    if (error)
      return;
  }

  // ADS: not sure this one is needed
  if (!log_file.empty()) {
    const auto log_file_path = fs::path(log_file);
    // If the path isn't absolute, we already have the directory by
    // definition
    if (log_file_path.is_absolute()) {
      const auto log_file_dir = log_file_path.parent_path().string();
      create(log_file_dir);
      if (error)
        return;
    }
  }
}

[[nodiscard]] auto
client_config::read_config_file(const std::string &config_file,
                                std::error_code &error) noexcept
  -> client_config {
  std::ifstream in(config_file);
  if (!in) {
    error = client_config_error_code::failed_to_read_client_config_file;
    return {};
  }
  const nlohmann::json data = nlohmann::json::parse(in, nullptr, false);
  if (data.is_discarded()) {
    error = client_config_error_code::failed_to_parse_client_config_file;
    return {};
  }
  client_config config;
  try {
    config = data;
  }
  catch (const std::exception &e) {
    error = client_config_error_code::invalid_client_config_file;
    return {};
  }
  return config;
}

[[nodiscard]] auto
client_config::read(std::string config_dir,
                    std::error_code &error) noexcept -> client_config {
  namespace fs = std::filesystem;
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

  auto config = read_config_file(config_file, error);
  if (config.config_dir.empty())
    config.config_dir = config_dir;
  return config;
}

auto
client_config::read_config_file_no_overwrite(std::error_code &error) noexcept
  -> void {
  const auto tmp = client_config::read(config_dir, error);
  if (error)
    return;
  if (config_dir.empty())
    config_dir = tmp.config_dir;
  if (hostname.empty())
    hostname = tmp.hostname;
  if (port.empty())
    port = tmp.port;
  if (index_dir.empty())
    index_dir = tmp.index_dir;
  if (metadata_file.empty())
    metadata_file = tmp.metadata_file;
  if (methylome_dir.empty())
    methylome_dir = tmp.methylome_dir;
  if (log_file.empty())
    log_file = tmp.log_file;
}

auto
client_config::save(std::error_code &error) const noexcept -> void {
  assert(!config_dir.empty());

  std::filesystem::create_directories(config_dir, error);
  if (error)
    return;

  const auto config_file = get_config_file(config_dir, error);
  if (error)
    return;

  const bool file_exists = std::filesystem::exists(config_file, error);
  if (error)
    return;

  // ADS: need to extract this pattern of updates intead of writes.
  client_config tmp = *this;
  if (file_exists) {
    tmp = client_config::read_config_file(config_file, error);
    if (error)
      return;
    // assign variables to tmp if they are not empty
    if (!config_dir.empty())
      tmp.config_dir = config_dir;
    if (!hostname.empty())
      tmp.hostname = hostname;
    if (!port.empty())
      tmp.port = port;
    if (!index_dir.empty())
      tmp.index_dir = index_dir;
    if (!metadata_file.empty())
      tmp.metadata_file = metadata_file;
    if (!methylome_dir.empty())
      tmp.methylome_dir = methylome_dir;
    if (!log_file.empty())
      tmp.log_file = log_file;
    // always overwrite log level -- no way to know not to
    tmp.log_level = log_level;
  }

  std::ofstream out(config_file);
  if (!out) {
    error = client_config_error_code::error_writing_config_file;
    return;
  }
  const std::string payload = tmp.tostring();
  out.write(payload.data(), static_cast<std::streamsize>(std::size(payload)));
  if (!out)
    error = client_config_error_code::error_writing_config_file;
}

[[nodiscard]] auto
client_config::tostring() const -> std::string {
  static constexpr auto n_indent = 4;
  nlohmann::json data = *this;
  return std::format("{}\n", data.dump(n_indent));
}

[[nodiscard]] static auto
dl_err(const auto &hdr, const auto &ec, const auto &url) -> std::error_code {
  auto &lgr = transferase::logger::instance();
  lgr.debug("Error downloading {}: ", url);
  if (ec)
    lgr.debug("Error code: {}", ec);
  const auto status_itr = hdr.find("status");
  if (status_itr != std::cend(hdr))
    lgr.debug("HTTP status: {}", status_itr->second);
  return ec;
}

[[nodiscard]] static auto
check_is_outdated(const download_request &dr,
                  const std::filesystem::path &local_file,
                  std::error_code &error) -> bool {
  const auto local_file_exists = std::filesystem::exists(local_file, error);
  if (error)
    return false;
  if (!local_file_exists)
    return true;

  const std::filesystem::file_time_type ftime =
    std::filesystem::last_write_time(local_file, error);
  if (error)
    return false;
  const auto remote_timestamp = get_timestamp(dr);

  return (remote_timestamp - ftime).count() > 0;
}

[[nodiscard]] static auto
download_index_files(const remote_data_resource &remote,
                     const std::vector<std::string> &genomes,
                     const std::string &dirname,
                     const download_policy_t download_policy,
                     const bool show_progress) -> std::error_code {
  auto &lgr = transferase::logger::instance();
  for (const auto &genome : genomes) {
    const auto stem = remote.form_index_target_stem(genome);
    const auto data_file =
      std::format("{}{}", stem, genome_index_data::filename_extension);

    const download_request dr{
      remote.hostname, remote.port, data_file, dirname, show_progress,
    };

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

      const auto [data_hdr, data_err] = download(dr);
      if (data_err)
        return dl_err(data_hdr, data_err, remote.form_url(data_file));

      const auto meta_file =
        std::format("{}{}", stem, genome_index_metadata::filename_extension);
      lgr.debug(R"(Download: {} to "{}")", remote.form_url(meta_file), dirname);
      const auto [meta_hdr, meta_err] =
        download({remote.hostname, remote.port, meta_file, dirname});
      if (meta_err)
        return dl_err(meta_hdr, meta_err, remote.form_url(meta_file));
    }
  }
  return {};
}

[[nodiscard]] static auto
download_metadata_file(
  const remote_data_resource &remote, const std::string &dirname,
  const download_policy_t download_policy) -> std::error_code {
  const auto metadata_file = remote.form_metadata_target();
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
    remote.hostname,
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
client_config::install(const std::vector<std::string> &genomes,
                       const download_policy_t download_policy,
                       std::string sys_config_dir,
                       const bool show_progress) const -> void {
  namespace fs = std::filesystem;
  auto &lgr = transferase::logger::instance();
  assert(!config_dir.empty());
  if (sys_config_dir.empty())
    sys_config_dir = get_default_system_config_dirname();

  std::error_code error;
  const bool valid = validate(error);
  if (!valid || error)
    throw std::system_error(error, "[Calling validate]");

  lgr.debug("Making configuration directories");
  lgr.debug("progress {}", show_progress);
  make_directories(error);
  if (error)
    throw std::system_error(
      client_config_error_code::error_creating_directories, config_dir);

  lgr.debug("Writing configuration file");
  save(error);
  if (error) {
    lgr.debug("Error writing config file: {}", error);
    return;
  }

  const system_config sys_conf(sys_config_dir);
  const auto &remotes = sys_conf.get_remote_resources();
  const auto metadata_dir =
    (fs::path(config_dir) / metadata_file).parent_path().string();
  // Do the downloads, attempting each remote resources server in the
  // system config
  bool metadata_downloads_ok{false};
  for (const auto &remote : remotes) {
    const auto metadata_err =
      download_metadata_file(remote, metadata_dir, download_policy);
    if (metadata_err)
      lgr.debug("Error obtaining metadata file: {}", metadata_err);
    if (!metadata_err) {
      metadata_downloads_ok = true;
      break;
    }
  }
  if (!metadata_downloads_ok)
    throw std::system_error(client_config_error_code::metadata_download_error);

  if (genomes.empty())
    return;

  const auto index_full_path = (fs::path(config_dir) / index_dir).string();

  bool genome_downloads_ok{false};
  for (const auto &remote : remotes) {
    const auto index_err = download_index_files(
      remote, genomes, index_full_path, download_policy, show_progress);
    if (index_err)
      lgr.debug("Error obtaining index files: {}", index_err);
    if (!index_err) {
      genome_downloads_ok = true;
      break;
    }
  }
  if (!genome_downloads_ok)
    throw std::system_error(
      client_config_error_code::genome_index_download_error);

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

auto
client_config::load_transferase_metadata(std::error_code &error) -> void {
  meta = transferase_metadata::read(get_metadata_file(), error);
}

[[nodiscard]] auto
client_config::config_file_exists() const -> bool {
  return !config_dir.empty() &&
         std::filesystem::exists(get_config_file(config_dir));
}

}  // namespace transferase
