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

#include "server_config.hpp"
#include "config_file_utils.hpp"  // for transferase::parse_config_file

#include <boost/json.hpp>            // for boost::json::operator<<, boost::...
#include <boost/mp11/algorithm.hpp>  // for boost::mp11::mp_for_each

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdlib>  // for getenv
#include <filesystem>
#include <range>  // IWYU pragma: keep
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>

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
server_config::get_default_config_dir(std::error_code &error) -> std::string {
  static const auto config_dir_rhs =
    std::filesystem::path(".config/transferase");
  static const auto env_home = std::getenv("HOME");
  if (!env_home) {
    error = std::make_error_code(std::errc{errno});
    return {};
  }
  const std::filesystem::path config_dir = env_home / config_dir_rhs;
  return config_dir;
}

[[nodiscard]] auto
server_config::get_dir_default_impl(const std::string &dirname,
                                    std::error_code &ec) -> std::string {
  const auto config_dir_local = get_default_config_dir(ec);
  if (ec)
    return {};
  return check_and_return_directory(config_dir_local, dirname, ec);
}

[[nodiscard]] auto
server_config::get_file_default_impl(const std::string &filename,
                                     std::error_code &ec) -> std::string {
  const auto config_dir_local = get_default_config_dir(ec);
  if (ec)
    return {};
  return check_and_return_file(config_dir_local, filename, ec);
}

[[nodiscard]] auto
server_config::get_config_file(const std::string &config_dir,
                               std::error_code &error) -> std::string {
  return check_and_return_file(config_dir, server_config_filename_default,
                               error);
}

auto
server_config::read_config_file_no_overwrite(
  const std::string &config_file, std::error_code &error) noexcept -> void {
  namespace fs = std::filesystem;
  // If config dir is empty, get the default
  if (config_file.empty()) {
    error = std::make_error_code(std::errc::no_such_file_or_directory);
    return;
  }

  // Parse as vector of pairs of strings
  const auto key_vals = parse_config_file_as_key_val(config_file, error);
  if (error)
    return;

  // Convert to unordered map
  std::unordered_map<std::string, std::string> key_val_map;
  std::ranges::for_each(
    key_vals, [&key_val_map](const auto &kv) { key_val_map.insert(kv); });

  const auto do_assign_if_empty = [&](auto &var, std::string name) {
    std::ranges::replace(name, '_', '-');
    if (key_val_map.contains(name) && var.empty())
      var = key_val_map[name];
  };
  do_assign_if_empty(config_dir, "config_dir");
  do_assign_if_empty(hostname, "hostname");
  do_assign_if_empty(port, "port");
  do_assign_if_empty(methylome_dir, "methylome_dir");
  do_assign_if_empty(index_dir, "index_dir");
  do_assign_if_empty(log_file, "log_file");
  do_assign_if_empty(pid_file, "pid_file");

  const auto do_assign_if_zero = [&](auto &var, std::string name) {
    std::ranges::replace(name, '_', '-');
    if (key_val_map.contains(name) && var == 0)
      std::istringstream(key_val_map[name]) >> var;
  };
  do_assign_if_zero(n_threads, "n_threads");
  do_assign_if_zero(max_resident, "max_resident");
  do_assign_if_zero(min_bin_size, "min_bin_size");
  do_assign_if_zero(max_intervals, "max_intervals");
}

[[nodiscard]] auto
server_config::read(const std::string &config_file,
                    std::error_code &error) noexcept -> server_config {
  server_config tmp;
  parse_config_file(tmp, config_file, error);
  if (error)
    return {};
  return tmp;
}

auto
server_config::write(const std::string &config_dir,
                     std::error_code &error) const noexcept -> void {
  assert(!config_dir.empty());
  const auto config_file = get_config_file(config_dir, error);
  if (error)
    return;

  const bool file_exists = std::filesystem::exists(config_file, error);
  if (error)
    return;

  server_config tmp = *this;
  if (file_exists) {
    // load current config values into a separate object
    tmp = read(config_dir, error);
    if (error)
      return;
    using Md =
      boost::describe::describe_members<server_config,
                                        boost::describe::mod_any_access>;
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
    error = server_config_error_code::error_writing_config_file;
}

[[nodiscard]] auto
server_config::tostring() const -> std::string {
  std::ostringstream o;
  if (!(o << boost::json::value_from(*this)))
    o.clear();
  return o.str();
}

/// Validate that the client config makes sense. This must be done
/// before creating directories, writing config files or attempting to
/// download any config data.
[[nodiscard]] auto
server_config::validate(std::error_code &error) const noexcept -> bool {
  static constexpr auto max_n_threads = 256;
  static constexpr auto max_max_resident = 8192;

  if (hostname.empty()) {
    error = server_config_error_code::invalid_server_config_information;
    return false;
  }

  if (port.empty()) {
    error = server_config_error_code::invalid_server_config_information;
    return false;
  }

  {
    // validate the port
    std::istringstream is(port);
    std::uint16_t port_value{};
    if (!(is >> port_value)) {
      error = server_config_error_code::invalid_server_config_information;
      return false;
    }
  }

  if (index_dir.empty()) {
    error = server_config_error_code::invalid_server_config_information;
    return false;
  }

  if (n_threads == 0 || n_threads > max_n_threads) {
    error = server_config_error_code::invalid_server_config_information;
    return false;
  }

  if (max_resident == 0 || max_resident > max_max_resident) {
    error = server_config_error_code::invalid_server_config_information;
    return false;
  }

  return true;
}

}  // namespace transferase
