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

#ifndef SRC_SERVER_CONFIG_HPP_
#define SRC_SERVER_CONFIG_HPP_

// ADS: Not sure why logger is needed below, but it might be that the
// full logger needs to be seen for 'mp11' and 'describe'

#include "logger.hpp"  // IWYU pragma: keep

#include <boost/describe.hpp>

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <utility>      // for std::to_underlying, std::unreachable
#include <vector>

namespace transferase {

struct server_config {
  static constexpr auto server_config_filename_default =
    "transferase_server_config.toml";

  std::string hostname;
  std::string port;
  std::string methylome_dir;
  std::string index_dir;
  std::string log_file;
  std::string pid_file;
  std::string log_level;
  std::string n_threads;
  std::string max_resident;
  std::string min_bin_size;
  std::string max_intervals;

  /// Read the server configuration.
  [[nodiscard]] static auto
  read(const std::string &config_file,
       std::error_code &error) noexcept -> server_config;

  /// Set the server configuration by writing the config file.
  auto
  configure(const std::string &config_file,
            std::error_code &error) const noexcept -> void;

  [[nodiscard]] static auto
  get_default_config_dir(std::error_code &error) -> std::string;

  [[nodiscard]] static auto
  get_config_file(const std::string &config_dir,
                  std::error_code &error) -> std::string;

  [[nodiscard]] auto
  tostring() const -> std::string;

  /// Validate that the required instance variables are set properly;
  /// do this before attempting the configuration process.
  [[nodiscard]] auto
  validate(std::error_code &error) const noexcept -> bool;

  /// Write the server configuration to the given configuration
  /// directory.
  auto
  write(const std::string &config_dir,
        std::error_code &error) const noexcept -> void;

private:
  [[nodiscard]] auto
  get_file_default_impl(const std::string &filename,
                        std::error_code &error) -> std::string;

  [[nodiscard]] auto
  get_dir_default_impl(const std::string &dirname,
                       std::error_code &error) -> std::string;
};

// clang-format off
BOOST_DESCRIBE_STRUCT(server_config, (), (
  hostname,
  port,
  methylome_dir,
  index_dir,
  log_file,
  pid_file,
  log_level,
  n_threads,
  max_resident,
  min_bin_size,
  max_intervals
)
)
// clang-format on

}  // namespace transferase

/// @brief Enum for error codes related to server configuration
enum class server_config_error_code : std::uint8_t {
  ok = 0,
  error_writing_config_file = 1,
  invalid_server_config_information = 2,
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
    case 1: return "error writing config file"s;
    case 2: return "invalid server config information"s;
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

#endif  // SRC_SERVER_CONFIG_HPP_
