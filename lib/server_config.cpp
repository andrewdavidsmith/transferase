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

#include "nlohmann/json.hpp"

#include <cassert>
#include <cerrno>
#include <cstdlib>  // for getenv
#include <filesystem>
#include <fstream>
#include <iterator>  // for std::size
#include <ranges>    // IWYU pragma: keep
#include <sstream>
#include <string>
#include <system_error>

namespace transferase {

auto
server_config::make_paths_absolute() noexcept -> void {
  namespace fs = std::filesystem;
  // errors in absolute are for std::bad_alloc
  std::error_code ignored_error;
  if (!index_dir.empty())
    index_dir = fs::absolute(index_dir, ignored_error).string();
  if (!methylome_dir.empty())
    methylome_dir = fs::absolute(methylome_dir, ignored_error).string();
  if (!log_file.empty())
    log_file = fs::absolute(log_file, ignored_error).string();
  if (!pid_file.empty())
    pid_file = fs::absolute(pid_file, ignored_error).string();
  assert(!ignored_error);
}

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

/// Get the path to the config file (base on dir)
[[nodiscard]] auto
server_config::get_config_file(const std::string &config_dir) noexcept
  -> std::string {
  return (std::filesystem::path(config_dir) / server_config_filename_default)
    .lexically_normal();
}

/// Get the path to the index directory
[[nodiscard]] auto
server_config::get_index_dir() const noexcept -> std::string {
  return (std::filesystem::path(config_dir) / index_dir).lexically_normal();
}

/// Get the path to the methylome directory
[[nodiscard]] auto
server_config::get_methylome_dir() const noexcept -> std::string {
  return (std::filesystem::path(config_dir) / methylome_dir).lexically_normal();
}

/// Get the path to the log file
[[nodiscard]] auto
server_config::get_log_file() const noexcept -> std::string {
  return (std::filesystem::path(config_dir) / log_file).lexically_normal();
}

[[nodiscard]] auto
server_config::read(const std::string &config_file,
                    std::error_code &error) noexcept -> server_config {
  std::ifstream in(config_file);
  if (!in) {
    error = server_config_error_code::failed_to_read_server_config_file;
    return {};
  }
  const nlohmann::json data = nlohmann::json::parse(in, nullptr, false);
  if (data.is_discarded()) {
    error = server_config_error_code::failed_to_parse_server_config_file;
    return {};
  }
  server_config config;
  try {
    config = data;
  }
  catch (const nlohmann::json::exception &e) {
    error = server_config_error_code::invalid_server_config_file;
    return {};
  }
  return config;
}

auto
server_config::read_config_file_no_overwrite(
  const std::string &config_file, std::error_code &error) noexcept -> void {
  const auto tmp = server_config::read(config_file, error);
  if (error)
    return;

  if (config_dir.empty())
    config_dir = tmp.config_dir;
  if (hostname.empty())
    hostname = tmp.hostname;
  if (port == 0)
    port = tmp.port;
  if (index_dir.empty())
    index_dir = tmp.index_dir;
  if (methylome_dir.empty())
    methylome_dir = tmp.methylome_dir;
  if (log_file.empty())
    log_file = tmp.log_file;
  if (pid_file.empty())
    pid_file = tmp.pid_file;

  if (n_threads != 0)
    n_threads = tmp.n_threads;
  if (max_resident != 0)
    max_resident = tmp.max_resident;
  if (min_bin_size != 0)
    min_bin_size = tmp.min_bin_size;
  if (max_intervals != 0)
    max_intervals = tmp.max_intervals;
}

[[nodiscard]] auto
server_config::tostring() const -> std::string {
  static constexpr auto n_indent = 4;
  nlohmann::json data = *this;
  return data.dump(n_indent);
}

[[nodiscard]] auto
server_config::write(const std::string &config_file) const -> std::error_code {
  std::ofstream out(config_file);
  if (!out)
    return server_config_error_code::error_writing_server_config_file;
  const std::string payload = tostring();
  out.write(payload.data(), static_cast<std::streamsize>(std::size(payload)));
  if (!out)
    return server_config_error_code::error_writing_server_config_file;
  return {};
}

/// Validate that the client config makes sense. This must be done
/// before creating directories, writing config files or attempting to
/// download any config data.
[[nodiscard]] auto
server_config::validate(std::error_code &error) const noexcept -> bool {
  if (hostname.empty()) {
    error = server_config_error_code::invalid_server_config_information;
    return false;
  }

  if (port == 0) {
    error = server_config_error_code::invalid_server_config_information;
    return false;
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

  if (min_bin_size == 0) {
    error = server_config_error_code::invalid_server_config_information;
    return false;
  }

  if (max_intervals == 0) {
    error = server_config_error_code::invalid_server_config_information;
    return false;
  }

  return true;
}

}  // namespace transferase
