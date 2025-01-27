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

#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>

[[nodiscard]] static inline auto
check_and_return_directory(const std::string &left, const std::string &right,
                           std::error_code &ec) -> std::string {
  const auto dirname = std::filesystem::path{left} / right;
  const bool exists = std::filesystem::exists(dirname, ec);
  if (ec)
    return {};
  if (!exists)
    return dirname;
  const auto is_dir = std::filesystem::is_directory(dirname, ec);
  if (ec)
    return {};
  if (!is_dir) {
    ec = std::make_error_code(std::errc::not_a_directory);
    return {};
  }
  return dirname;
}

[[nodiscard]] static inline auto
check_and_return_directory(const std::string &dirname,
                           std::error_code &ec) -> std::string {
  const bool exists = std::filesystem::exists(dirname, ec);
  if (ec)
    return {};
  if (!exists)
    return dirname;
  const auto is_dir = std::filesystem::is_directory(dirname, ec);
  if (ec)
    return {};
  if (!is_dir) {
    ec = std::make_error_code(std::errc::not_a_directory);
    return {};
  }
  return dirname;
}

[[nodiscard]] static inline auto
create_dirs_if_needed(const std::string &dirname,
                      std::error_code &error) -> bool {
  const bool exists = std::filesystem::exists(dirname, error);
  if (!exists) {
    const bool dirs_ok = std::filesystem::create_directories(dirname, error);
    if (error)
      return false;
    return dirs_ok;
  }
  const auto is_dir = std::filesystem::is_directory(dirname, error);
  if (error)
    return false;
  return is_dir;
}

// [[nodiscard]] static inline auto
// check_and_return_existing_file(const std::string &left,
//                                const std::string &right,
//                                std::error_code &ec) -> std::string {
//   const auto joined = std::filesystem::path{left} / right;
//   const auto file_exists = std::filesystem::exists(joined, ec);
//   if (ec)
//     return {};
//   if (!file_exists) {
//     ec = std::make_error_code(std::errc::no_such_file_or_directory);
//     return {};
//   }
//   std::error_code ignored_ec;
//   const auto is_dir = std::filesystem::is_directory(joined, ignored_ec);
//   if (is_dir) {
//     ec = std::make_error_code(std::errc::is_a_directory);
//     return {};
//   }
//   return joined;
// }

[[nodiscard]] static inline auto
check_and_return_file(const std::string &left, const std::string &right,
                      std::error_code &ec) -> std::string {
  const auto joined = std::filesystem::path{left} / right;
  std::error_code ignored_ec;
  const auto is_dir = std::filesystem::is_directory(joined, ignored_ec);
  if (is_dir) {
    ec = std::make_error_code(std::errc::is_a_directory);
    return {};
  }
  return joined;
}

[[nodiscard]] inline auto
clean_path(const std::string &s, std::error_code &ec) -> std::string {
  auto p = std::filesystem::absolute(s, ec);
  if (ec)
    return {};
  p = std::filesystem::weakly_canonical(p, ec);
  if (ec)
    return {};
  return p.string();
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
client_config::get_labels_dir_default(std::error_code &ec) -> std::string {
  const auto config_dir = get_config_dir_default(ec);
  if (ec)
    return {};
  return check_and_return_directory(config_dir, labels_dirname, ec);
}

[[nodiscard]] auto
client_config::get_index_dir_default(std::error_code &ec) -> std::string {
  const auto config_dir = get_config_dir_default(ec);
  if (ec)
    return {};
  return check_and_return_directory(config_dir, index_dirname, ec);
}

[[nodiscard]] auto
client_config::get_config_file_default(std::error_code &ec) -> std::string {
  const auto config_dir = get_config_dir_default(ec);
  if (ec)
    return {};
  return check_and_return_file(config_dir, client_config_filename, ec);
}

[[nodiscard]] auto
client_config::get_log_file_default(std::error_code &ec) -> std::string {
  const auto config_dir = get_config_dir_default(ec);
  if (ec)
    return {};
  return check_and_return_file(config_dir, client_log_filename, ec);
}

[[nodiscard]] auto
client_config::get_labels_dir_default_impl(std::error_code &ec) -> std::string {
  const auto config_dir_local =
    config_dir.empty() ? get_config_dir_default(ec) : config_dir;
  if (ec)
    return {};
  return check_and_return_directory(config_dir_local, labels_dirname, ec);
}

[[nodiscard]] auto
client_config::get_index_dir_default_impl(std::error_code &ec) -> std::string {
  const auto config_dir_local =
    config_dir.empty() ? get_config_dir_default(ec) : config_dir;
  if (ec)
    return {};
  return check_and_return_directory(config_dir_local, index_dirname, ec);
}

[[nodiscard]] auto
client_config::get_config_file_default_impl(std::error_code &ec)
  -> std::string {
  const auto config_dir_local =
    config_dir.empty() ? get_config_dir_default(ec) : config_dir;
  if (ec)
    return {};
  return check_and_return_file(config_dir_local, client_config_filename, ec);
}

[[nodiscard]] auto
client_config::get_log_file_default_impl(std::error_code &ec) -> std::string {
  const auto config_dir_local =
    config_dir.empty() ? get_config_dir_default(ec) : config_dir;
  if (ec)
    return {};
  return check_and_return_file(config_dir_local, client_log_filename, ec);
}

[[nodiscard]] auto
client_config::set_defaults(const bool force) -> std::error_code {
  std::error_code ec;
  if (force || config_file.empty()) {
    config_file = get_config_file_default_impl(ec);
    if (ec)
      return ec;
  }

  if (force || index_dir.empty()) {
    index_dir = get_index_dir_default_impl(ec);
    if (ec)
      return ec;
  }

  if (force || labels_dir.empty()) {
    labels_dir = get_labels_dir_default_impl(ec);
    if (ec)
      return ec;
  }
  return std::error_code{};
}

[[nodiscard]] auto
client_config::set_config_file(const std::string &s) -> std::error_code {
  std::error_code ec;
  config_file = clean_path(s, ec);
  return ec;
}

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
client_config::init_config_file(const std::string &s,
                                std::error_code &error) -> void {
  config_file =
    s.empty() ? get_config_file_default_impl(error) : clean_path(s, error);
}

auto
client_config::init_log_file(const std::string &s,
                             std::error_code &error) -> void {
  log_file =
    s.empty() ? get_log_file_default_impl(error) : clean_path(s, error);
}

auto
client_config::init_index_dir(const std::string &s,
                              std::error_code &error) -> void {
  if (s.empty()) {
    index_dir = get_index_dir_default_impl(error);
    if (error)
      return;
  }
  else
    index_dir = s;
}

auto
client_config::init_labels_dir(const std::string &s,
                               std::error_code &error) -> void {
  if (s.empty()) {
    labels_dir = get_labels_dir_default_impl(error);
    if (error)
      return;
  }
  else
    labels_dir = s;
}

[[nodiscard]] auto
client_config::set_hostname(const std::string &s) -> std::error_code {
  // ADS(TODO): check hostname
  hostname = s;
  return {};
}

[[nodiscard]] auto
client_config::set_port(const std::string &s) -> std::error_code {
  std::istringstream is(s);
  std::uint16_t port_parsed{};
  if (!(is >> port_parsed)) {
    port.clear();
    return std::make_error_code(std::errc::result_out_of_range);
  }
  port = s;
  return {};
}

[[nodiscard]] auto
client_config::set_index_dir(const std::string &s) -> std::error_code {
  std::error_code ec;
  index_dir = clean_path(s, ec);
  return ec;
}

[[nodiscard]] auto
client_config::set_labels_dir(const std::string &s) -> std::error_code {
  std::error_code ec;
  labels_dir = clean_path(s, ec);
  return ec;
}

[[nodiscard]] auto
client_config::set_log_file(const std::string &s) -> std::error_code {
  assert(!s.empty());
  std::error_code ec;
  log_file = clean_path(s, ec);
  return ec;
}

[[nodiscard]] auto
client_config::set_log_level(const log_level_t ll) -> std::error_code {
  // ADS(TOD): validate log level
  log_level = ll;
  return {};
}

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
  out.write(serialized.data(), std::size(serialized));
  if (!out)
    return std::make_error_code(std::errc::bad_file_descriptor);
  return std::error_code{};
}

}  // namespace transferase
