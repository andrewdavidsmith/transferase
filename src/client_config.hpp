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

#include "logger.hpp"  // for transferase::log_level_t

#include <boost/describe.hpp>

#include <string>
#include <system_error>

namespace transferase {

class client_config {
public:
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

private:
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
  hostname,
  port,
  index_dir,
  labels_dir,
  methylome_dir,
  log_file,
  log_level
))
// clang-format on

}  // namespace transferase

#endif  // SRC_CLIENT_CONFIG_HPP_
