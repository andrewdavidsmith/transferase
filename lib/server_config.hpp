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

#ifndef LIB_SERVER_CONFIG_HPP_
#define LIB_SERVER_CONFIG_HPP_

#include "logger.hpp"  // IWYU pragma: keep

#include "nlohmann/json.hpp"

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <utility>      // for std::to_underlying, std::unreachable

namespace transferase {

struct server_config {
  static constexpr auto max_n_threads = 1024;
  static constexpr auto max_max_resident = 8192;
  static constexpr auto default_n_threads = 1;
  static constexpr auto default_max_resident = 128;
  static constexpr auto server_config_filename_default =
    "transferase_server.json";

  std::string config_dir;
  std::string hostname;
  std::string port{};
  std::string methylome_dir;
  std::string index_dir;
  std::string log_file;
  std::string pid_file;
  log_level_t log_level{};
  std::uint32_t n_threads{};
  std::uint32_t max_resident{};
  std::uint32_t min_bin_size{};
  std::uint32_t max_intervals{};

  /// Get the path to the index directory
  [[nodiscard]] auto
  get_index_dir() const noexcept -> std::string;

  /// Get the path to the methylome directory
  [[nodiscard]] auto
  get_methylome_dir() const noexcept -> std::string;

  /// Get the path to the log file
  [[nodiscard]] auto
  get_log_file() const noexcept -> std::string;

  /// Initialize any empty values by reading the config file
  auto
  read_config_file_no_overwrite(const std::string &config_file,
                                std::error_code &error) noexcept -> void;

  /// Read the server configuration.
  [[nodiscard]] static auto
  read(const std::string &config_file,
       std::error_code &error) noexcept -> server_config;

  /// Write the configuration to a file
  [[nodiscard]] auto
  write(const std::string &config_file) const -> std::error_code;

  /// Set the server configuration by writing the config file.
  auto
  configure(const std::string &config_file,
            std::error_code &error) const noexcept -> void;

  [[nodiscard]] static auto
  get_default_config_dir(std::error_code &error) -> std::string;

  [[nodiscard]] static auto
  get_config_file(const std::string &config_dir) noexcept -> std::string;

  [[nodiscard]] auto
  tostring() const -> std::string;

  auto
  make_paths_absolute() noexcept -> void;

  /// Validate that the required instance variables are set properly;
  /// do this before attempting the configuration process.
  [[nodiscard]] auto
  validate(std::error_code &error) const noexcept -> bool;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(server_config, config_dir, hostname, port,
                                 methylome_dir, index_dir, log_file, pid_file,
                                 log_level, n_threads, max_resident,
                                 min_bin_size, max_intervals)
};

}  // namespace transferase

/// @brief Enum for error codes related to server configuration
enum class server_config_error_code : std::uint8_t {
  ok = 0,
  error_writing_server_config_file = 1,
  invalid_server_config_information = 2,
  failed_to_read_server_config_file = 3,
  failed_to_parse_server_config_file = 4,
  invalid_server_config_file = 5,
};

template <>
struct std::is_error_code_enum<server_config_error_code>
  : public std::true_type {};

struct server_config_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override {return "server_config";}
  auto message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "error writing server config file"s;
    case 2: return "invalid server config information"s;
    case 3: return "failed to read server config file"s;
    case 4: return "failed to parse server config file"s;
    case 5: return "invalid server config file"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(server_config_error_code e) -> std::error_code {
  static auto category = server_config_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // LIB_SERVER_CONFIG_HPP_
