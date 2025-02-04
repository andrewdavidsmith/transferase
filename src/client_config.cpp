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
#include "find_path_to_binary.hpp"
#include "genome_index_data.hpp"
#include "genome_index_metadata.hpp"
#include "remote_data_resource.hpp"
#include "utilities.hpp"  // for clean_path

#include <config.h>

#include <boost/json.hpp>
#include <boost/mp11/algorithm.hpp>

#include <algorithm>  // for std::ranges::replace
#include <cerrno>
#include <chrono>  // for std::chrono::operator-
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
get_default_system_config_dir(std::error_code &error) noexcept -> std::string {
  static const auto exe_path = find_path_to_binary();
  auto &lgr = logger::instance();
  const auto exe_dir_parent =
    std::filesystem::canonical(exe_path).parent_path().parent_path().string();
  const bool is_dir = std::filesystem::is_directory(exe_dir_parent, error);
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
  const auto data_part = std::filesystem::path{DATADIR};
  const auto data_dir = exe_dir_parent / data_part / std::string(PROJECT_NAME);
  return data_dir;
}

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
  return check_and_return_directory(env_home_path,
                                    transferase_config_dirname_default, ec);
}

[[nodiscard]] auto
client_config::get_dir_default_impl(const std::string &dirname,
                                    std::error_code &ec) -> std::string {
  const auto config_dir_local = get_config_dir_default(ec);
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
  return check_and_return_directory(config_dir, index_dirname_default, error);
}

[[nodiscard]] auto
client_config::get_file_default_impl(const std::string &filename,
                                     std::error_code &ec) -> std::string {
  const auto config_dir_local = get_config_dir_default(ec);
  if (ec)
    return {};
  return check_and_return_file(config_dir_local, filename, ec);
}

[[nodiscard]] auto
client_config::get_config_file(const std::string &config_dir,
                               std::error_code &error) -> std::string {
  return check_and_return_file(config_dir, client_config_filename_default,
                               error);
}

[[nodiscard]] auto
client_config::set_defaults_system_config(const std::string &config_dir,
                                          const std::string &system_config_dir)
  -> std::error_code {
  std::error_code error;
  const auto [default_hostname, default_port] =
    transferase::get_transferase_server_info(system_config_dir, error);
  if (error)
    return error;

  hostname = default_hostname;
  port = default_port;

  {
    const auto dirname =
      (std::filesystem::path{config_dir} / index_dirname_default).string();
    index_dir = get_dir_default_impl(dirname, error);
    if (error)
      return error;
  }

  {
    const auto dirname =
      (std::filesystem::path{config_dir} / labels_dirname_default).string();
    labels_dir = get_dir_default_impl(dirname, error);
    if (error)
      return error;
  }

  return std::error_code{};
}

[[nodiscard]] auto
client_config::set_defaults_system_config(const std::string &system_config_dir)
  -> std::error_code {
  std::error_code error;
  const std::string config_dir = get_config_dir_default(error);
  if (error)
    return error;
  return set_defaults_system_config(config_dir, system_config_dir);
}

[[nodiscard]] auto
client_config::set_defaults(const std::string &config_dir) -> std::error_code {
  std::error_code error;
  const std::string system_config_dir = get_default_system_config_dir(error);
  if (error)
    return error;
  return set_defaults_system_config(config_dir, system_config_dir);
}

[[nodiscard]] auto
client_config::set_defaults() -> std::error_code {
  std::error_code error;
  const std::string config_dir = get_config_dir_default(error);
  if (error)
    return error;
  return set_defaults(config_dir);
}

/// Create all the directories involved in the client config, but if
/// filesystem entires already exist for those directory names,
/// don't do anything. If they happen to already exist but are files
/// and not directories, it will result in an error later.
[[nodiscard]] auto
client_config::make_directories(std::string config_dir) const
  -> std::error_code {
  std::error_code ec;

  config_dir = std::filesystem::absolute(config_dir, ec);
  if (ec)
    return ec;
  config_dir = std::filesystem::weakly_canonical(config_dir, ec);
  if (ec)
    return ec;

  assert(!config_dir.empty());

  const bool dirs_ok = create_dirs_if_needed(config_dir, ec);
  if (ec)
    return ec;
  if (!dirs_ok)
    return std::make_error_code(std::errc::not_a_directory);

  const auto config_file = get_config_file(config_dir, ec);
  if (ec)
    return ec;

  assert(!config_file.empty());

  const auto config_file_clean = clean_path(config_file, ec);
  if (ec)
    return ec;

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

template <class T>
auto
assign_member_impl(T &t, const std::string_view value) -> std::error_code {
  std::string tmp(std::cbegin(value), std::cend(value));
  std::istringstream is(tmp);
  if (!(is >> t))
    return std::make_error_code(std::errc::invalid_argument);
  return {};
}

template <class Scope>
auto
assign_member(Scope &scope, const std::string_view name,
              const std::string_view value) -> std::error_code {
  using Md =
    boost::describe::describe_members<Scope, boost::describe::mod_public>;
  std::error_code error{};
  boost::mp11::mp_for_each<Md>([&](const auto &D) {
    if (!error && name == D.name) {
      error = assign_member_impl(scope.*D.pointer, value);
    }
  });
  return error;
}

[[nodiscard]] inline auto
assign_members(const std::vector<std::tuple<std::string, std::string>> &key_val,
               client_config &cfg) -> std::error_code {
  std::error_code error;
  for (const auto &[key, value] : key_val) {
    std::string name(key);
    std::ranges::replace(name, '-', '_');
    error = assign_member(cfg, name, value);
    if (error)
      return {};
  }
  return error;
}

[[nodiscard]] auto
client_config::read(const std::string &config_dir,
                    std::error_code &error) noexcept -> client_config {
  const auto config_file = get_config_file(config_dir, error);
  if (error)
    return {};

  std::ifstream in(config_file);
  if (!in) {
    error = std::make_error_code(std::errc(errno));
    return {};
  }

  std::vector<std::tuple<std::string, std::string>> key_val;
  std::string line;
  while (getline(in, line)) {
    line = rlstrip(line);
    // ignore empty lines and those beginning with '#'
    if (!line.empty() && line[0] != '#') {
      const auto [k, v] = split_equals(line, error);
      if (error)
        return {};
      key_val.emplace_back(k, v);
    }
  }

  client_config c;
  error = assign_members(key_val, c);
  if (error)
    return {};

  return c;
}

[[nodiscard]] auto
client_config::read(std::error_code &error) noexcept -> client_config {
  const auto config_dir = get_config_dir_default(error);
  if (error)
    // error from get_config_dir_default
    return {};
  return read(config_dir, error);
}

[[nodiscard]] inline auto
format_as_client_config(const auto &t) -> std::string {
  using T = std::remove_cvref_t<decltype(t)>;
  using members =
    boost::describe::describe_members<T, boost::describe::mod_any_access>;
  std::string r;
  boost::mp11::mp_for_each<members>([&](const auto &member) {
    std::string name(member.name);
    std::ranges::replace(name, '_', '-');
    const auto value = std::format("{}", t.*member.pointer);
    if (!value.empty())
      r += std::format("{} = {}\n", name, value);
    else
      r += std::format("# {} =\n", name);
  });
  return r;
}

[[nodiscard]] auto
client_config::write(std::string config_dir) const -> std::error_code {
  std::error_code error;
  if (config_dir.empty()) {
    config_dir = get_config_dir_default(error);
    if (error)
      return error;
  }

  const auto config_file = get_config_file(config_dir, error);
  if (error)
    return error;

  const auto serialized = format_as_client_config(*this);
  const auto serialized_size =
    static_cast<std::streamsize>(std::size(serialized));

  std::ofstream out(config_file);
  if (!out)
    return std::make_error_code(std::errc(errno));

  out.write(serialized.data(), serialized_size);
  if (!out)
    return client_config_error_code::error_writing_config_file;

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
      lgr.debug(R"(Download: {} to "{}")", remote.form_url(data_file), dirname);

      const auto [data_hdr, data_err] =
        download({remote.host, remote.port, data_file, dirname});
      if (data_err)
        return dl_err(data_hdr, data_err, remote.form_url(data_file));

      const auto meta_file =
        std::format("{}{}", stem, genome_index_metadata::filename_extension);
      lgr.debug(R"(Download: {} to "{}")", remote.form_url(meta_file), dirname);
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
get_labels_file(const remote_data_resources &remote,
                const std::string &dirname) -> std::error_code {
  auto &lgr = transferase::logger::instance();
  const auto stem = remote.get_labels_target_stem();
  const auto labels_file = std::format("{}.json", stem);
  lgr.debug("Download: {} to {}", remote.form_url(labels_file), dirname);
  const auto [data_hdr, data_err] = download(download_request{
    remote.host,
    remote.port,
    labels_file,
    dirname,
  });
  if (data_err)
    return dl_err(data_hdr, data_err, remote.form_url(labels_file));
  return {};
}

auto
client_config::run(const std::string &config_dir,
                   const std::vector<std::string> &genomes,
                   const std::string &system_config_dir,
                   const bool force_download,
                   std::error_code &error) const noexcept -> void {
  auto &lgr = logger::instance();

  const bool valid = validate(error);
  if (!valid || error)
    return;

  lgr.debug("Making config directories");
  error = make_directories(config_dir);
  if (error) {
    error = client_config_error_code::error_creating_directories;
    lgr.debug("Error creating directories: {}", error);
    return;
  }

  lgr.debug("Writing configuration file");
  error = write(config_dir);
  if (error) {
    error = client_config_error_code::error_writing_config_file;
    lgr.debug("Error writing config file: {}", error);
    return;
  }

  const auto remotes =
    transferase::get_remote_data_resources(system_config_dir, error);
  if (error) {
    error = client_config_error_code::error_identifying_remote_resources;
    lgr.debug("Error identifying remote server: {}", error);
    return;
  }

  // Do the downloads, attempting each remote resources server in the
  // system config
  bool labels_downloads_ok{false};
  for (const auto &remote : remotes) {
    lgr.debug("Attempting download labels from {}:{}", remote.host,
              remote.port);
    const auto labels_err = get_labels_file(remote, labels_dir);
    if (labels_err)
      lgr.debug("Error obtaining labels file: {}", labels_err);
    if (!labels_err) {
      labels_downloads_ok = true;
      break;
    }
  }
  if (!labels_downloads_ok) {
    error = client_config_error_code::download_error;
    return;
  }

  if (genomes.empty())
    return;

  bool genome_downloads_ok{false};
  for (const auto &remote : remotes) {
    lgr.debug("Attempting download from {}:{}", remote.host, remote.port);
    const auto index_err =
      get_index_files(remote, genomes, index_dir, force_download);
    if (index_err)
      lgr.debug("Error obtaining index files: {}", index_err);
    if (!index_err) {
      genome_downloads_ok = true;
      break;
    }
  }
  if (!genome_downloads_ok)
    error = client_config_error_code::download_error;

  return;
}

auto
client_config::run(const std::string &config_dir,
                   const std::vector<std::string> &genomes,
                   const bool force_download,
                   std::error_code &error) const noexcept -> void {
  const auto system_config_dir = get_default_system_config_dir(error);
  if (error) {
    error = client_config_error_code::error_obtaining_sytem_config_dir;
    return;
  }
  run(config_dir, genomes, system_config_dir, force_download, error);
}

auto
client_config::run(const std::vector<std::string> &genomes,
                   const bool force_download,
                   std::error_code &error) const noexcept -> void {
  const auto config_dir = get_config_dir_default(error);
  if (error)
    return;
  run(config_dir, genomes, force_download, error);
}

auto
client_config::run(const std::vector<std::string> &genomes,
                   const std::string &system_config_dir,
                   const bool force_download,
                   std::error_code &error) const noexcept -> void {
  const auto config_dir = get_config_dir_default(error);
  if (error)
    return;
  run(config_dir, genomes, system_config_dir, force_download, error);
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
  // validate the labels_dir
  if (labels_dir.empty()) {
    error = client_config_error_code::invalid_client_config_information;
    return false;
  }
  return true;
}

}  // namespace transferase
