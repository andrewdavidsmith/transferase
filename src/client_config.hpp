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

#ifndef SRC_CLIENT_CONFIG_HPP_
#define SRC_CLIENT_CONFIG_HPP_

// ADS: Not sure why logger is needed below, but it might be that the
// full logger needs to be seen for 'mp11' and 'describe'
#include "logger.hpp"  // IWYU pragma: keep

#include <boost/describe.hpp>

#include <cstdint>
#include <format>  // for std::vector??
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <utility>      // for std::to_underlying, std::unreachable
#include <vector>

namespace transferase {

struct client_config {
  static constexpr auto transferase_config_dirname = ".config/transferase";
  static constexpr auto labels_dirname = "labels";
  static constexpr auto index_dirname = "indexes";
  static constexpr auto client_config_filename =
    "transferase_client_config.toml";
  static constexpr auto client_log_filename = "transferase.log";

  std::string config_dir;
  std::string config_file;

  std::string hostname;
  std::string port;
  std::string index_dir;
  std::string labels_dir;
  std::string methylome_dir;
  std::string log_file;
  log_level_t log_level{};

  auto
  run(const std::vector<std::string> &genomes, const bool force_download,
      std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  auto
  run(const std::vector<std::string> &genomes,
      const bool force_download) const -> void {
    std::error_code error;
    run(genomes, force_download, error);
    if (error)
      throw std::system_error(error);
  }
#endif

  auto
  run(const std::vector<std::string> &genomes, const std::string &data_dir,
      const bool force_download, std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  auto
  run(const std::vector<std::string> &genomes, const std::string &data_dir,
      const bool force_download) const -> void {
    std::error_code error;
    run(genomes, data_dir, force_download, error);
    if (error)
      throw std::system_error(error);
  }
#endif

  [[nodiscard]] auto
  validate(std::error_code &error) noexcept -> bool;

#ifndef TRANSFERASE_NOEXCEPT
  [[nodiscard]] auto
  validate() -> bool {
    std::error_code error;
    const auto validated = validate(error);
    if (error)
      throw std::system_error(error);
    return validated;
  }
#endif

  [[nodiscard]] auto
  write() const -> std::error_code;

  [[nodiscard]] auto
  make_directories() const -> std::error_code;

  [[nodiscard]] auto
  set_config_file(const std::string &) -> std::error_code;

  auto
  init_config_file(const std::string &s, std::error_code &error) -> void;

  auto
  init_config_dir(const std::string &s, std::error_code &error) -> void;

  auto
  init_index_dir(const std::string &s, std::error_code &error) -> void;

  auto
  init_labels_dir(const std::string &s, std::error_code &error) -> void;

  auto
  init_log_file(const std::string &s, std::error_code &error) -> void;

  [[nodiscard]] auto
  set_hostname(const std::string &) -> std::error_code;

  [[nodiscard]] auto
  set_port(const std::string &) -> std::error_code;

  [[nodiscard]] auto
  set_index_dir(const std::string &) -> std::error_code;

  [[nodiscard]] auto
  set_labels_dir(const std::string &) -> std::error_code;

  [[nodiscard]] auto
  set_log_file(const std::string &) -> std::error_code;

  [[nodiscard]] auto
  set_log_level(const log_level_t) -> std::error_code;

  [[nodiscard]] auto
  set_defaults(const bool force = false) -> std::error_code;

  [[nodiscard]] static auto
  get_config_dir_default(std::error_code &ec) -> std::string;

  [[nodiscard]] static auto
  get_config_file_default(std::error_code &ec) -> std::string;

  [[nodiscard]] static auto
  get_log_file_default(std::error_code &ec) -> std::string;

  [[nodiscard]] static auto
  get_index_dir_default(std::error_code &ec) -> std::string;

  [[nodiscard]] static auto
  get_labels_dir_default(std::error_code &ec) -> std::string;

  [[nodiscard]] auto
  tostring() const -> std::string;

  [[nodiscard]] auto
  get_config_file_default_impl(std::error_code &ec) -> std::string;

  [[nodiscard]] auto
  get_log_file_default_impl(std::error_code &ec) -> std::string;

  [[nodiscard]] auto
  get_index_dir_default_impl(std::error_code &ec) -> std::string;

  [[nodiscard]] auto
  get_labels_dir_default_impl(std::error_code &ec) -> std::string;
};

// clang-format off
BOOST_DESCRIBE_STRUCT(client_config, (), (
  config_dir,
  config_file,
  hostname,
  port,
  index_dir,
  labels_dir,
  methylome_dir,
  log_file,
  log_level
)
)
// clang-format on

}  // namespace transferase

/// @brief Enum for error codes related to client configuration
enum class client_config_error_code : std::uint8_t {
  ok = 0,
  not_ok = 1,
};

template <>
struct std::is_error_code_enum<client_config_error_code>
  : public std::true_type {};

struct client_config_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override {return "client_config";}
  auto message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "not ok"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(client_config_error_code e) -> std::error_code {
  static auto category = client_config_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_CLIENT_CONFIG_HPP_
