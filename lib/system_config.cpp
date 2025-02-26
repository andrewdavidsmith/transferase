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

#include "system_config.hpp"

#include "find_path_to_binary.hpp"

#include "nlohmann/json.hpp"

#include <config.h>  // for VERSION, DATADIR, PROJECT_NAME

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <stdexcept>  // std::runtime_error
#include <string>
#include <system_error>  // std::error_code and std::system_error
#include <vector>

namespace transferase {

[[nodiscard]] auto
get_system_config_filename() -> std::string {
  return std::format("{}_data_{}.json", PROJECT_NAME, VERSION);
}

/// Locate the system confuration directory based on the path to the binary for
/// the currently running process.
[[nodiscard]] auto
get_default_system_config_dirname() -> std::string {
  namespace fs = std::filesystem;
  const auto exe_path = find_path_to_binary();
  const auto exe_dir_parent =
    fs::canonical(exe_path).parent_path().parent_path().string();
  std::error_code error;
  const bool is_dir = fs::is_directory(exe_dir_parent, error);
  if (error || !is_dir)
    throw std::system_error(
      error, std::format("{} from {}", exe_dir_parent, exe_path));
  // ADS: DATADIR comes from config.h which comes from config.h.in and
  // is set by cmake
  return exe_dir_parent / fs::path{DATADIR} / std::string(PROJECT_NAME);
}

system_config::system_config(const std::string &data_dir) {
  std::error_code error;
  const bool is_dir = std::filesystem::is_directory(data_dir, error);
  if (error || !is_dir)
    throw std::system_error(error, data_dir);

  const auto json_file =
    (std::filesystem::path{data_dir} / get_system_config_filename()).string();

  std::ifstream in(json_file);
  if (!in) {
    error = std::make_error_code(std::errc(errno));
    throw std::system_error(error, json_file);
  }

  const nlohmann::json data = nlohmann::json::parse(in, nullptr, false);
  if (data.is_discarded())
    throw std::runtime_error(
      std::format("Failed to parse json from {}", json_file));

  system_config tmp;
  try {
    tmp = data;
  }
  catch (const nlohmann::json::exception &e) {
    throw std::runtime_error(std::format("{} [{}]", e.what(), json_file));
  }
  hostname = tmp.hostname;
  port = tmp.port;
  resources = tmp.resources;
}

}  // namespace transferase
