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

#include "remote_data_resource.hpp"
#include "find_path_to_binary.hpp"
#include "logger.hpp"

#include <config.h>  // for VERSION, DATADIR, PROJECT_NAME

#include <boost/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace transferase {

[[nodiscard]] auto
get_system_config_filename() -> std::string {
  return std::format("{}_data_{}.json", PROJECT_NAME, VERSION);
}

/// Locate the system confuration file with information about servers for data
/// downloads.
[[nodiscard]] static inline auto
get_remote_data_resources_impl(const std::string &data_dir,
                               std::error_code &error) noexcept
  -> std::vector<remote_data_resources> {
  auto &lgr = transferase::logger::instance();
  const bool is_dir = std::filesystem::is_directory(data_dir, error);
  if (error) {
    lgr.debug("Error: {} ({})", error, data_dir);
    return {};
  }
  if (!is_dir) {
    lgr.debug("Not a directory: {}", data_dir);
    return {};
  }

  const auto json_file =
    std::filesystem::path{data_dir} / get_system_config_filename();

  std::ifstream in(json_file);
  if (!in) {
    error = std::make_error_code(std::errc::not_a_directory);
    return {};
  }

  const auto filesize =
    static_cast<std::streamsize>(std::filesystem::file_size(json_file, error));
  if (error) {
    lgr.debug("Bad system config file: {}", json_file.string());
    return {};
  }

  std::string payload(filesize, '\0');
  if (!in.read(payload.data(), filesize)) {
    error = std::make_error_code(std::errc::bad_file_descriptor);
    lgr.debug("Failure reading file: {}", json_file.string());
    return {};
  }

  std::vector<remote_data_resources> resources;
  boost::json::parse_into(resources, payload, error);
  if (error) {
    lgr.debug("Malformed JSON for remote resources {}: {}", json_file.string(),
              error);
    return {};
  }
  return resources;
}

/// Locate the system confuration file with information about servers for data
/// downloads.
[[nodiscard]] auto
get_remote_data_resources(std::error_code &error) noexcept
  -> std::vector<remote_data_resources> {
  // ADS: this function prints error messages itself, so no need to
  // log based on error code returned
  static const auto exe_path = find_path_to_binary();
  auto &lgr = transferase::logger::instance();

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
  const auto data_path = std::filesystem::path{DATADIR};
  const auto data_dir = exe_dir_parent / data_path / std::string(PROJECT_NAME);

  return get_remote_data_resources_impl(data_dir, error);
}

/// Locate the system confuration file, using the specified data
/// directory, with information about servers for data downloads.
[[nodiscard]] auto
get_remote_data_resources(const std::string &data_dir,
                          std::error_code &error) noexcept
  -> std::vector<remote_data_resources> {
  return get_remote_data_resources_impl(data_dir, error);
}

}  // namespace transferase
