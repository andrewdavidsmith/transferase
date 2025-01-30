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
#include <vector>       // IWYU pragma: keep

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

  /// Does the work of configuring the client.
  ///
  /// This work includes creating directories, downloading files and
  /// writing files. The user's system is not changed until this point
  /// in the client configuration process.
  auto
  run(const std::vector<std::string> &genomes, const bool force_download,
      std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  /// Overload of ``run`` that throws system_error exceptions to serve
  /// in an API.
  auto
  run(const std::vector<std::string> &genomes,
      const bool force_download) const -> void {
    std::error_code error;
    run(genomes, force_download, error);
    if (error)
      throw std::system_error(error);
  }
#endif

  /// Does the work of configuring the client, accepting a directory
  /// to use when locating the system configuration file.
  ///
  /// This is an overload that can be used when the system
  /// configuration file cannot be expected to reside in a specific
  /// directory relative to the binary associated with the current
  /// process.
  auto
  run(const std::vector<std::string> &genomes,
      const std::string &system_config_dir, const bool force_download,
      std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  /// Overload of run that throw system_error exceptions to serve in
  /// an API, providing an interface when system configuration files
  /// reside in a language-specific location.
  ///
  /// This is currently used for Python bindings, which themselves use
  /// the Python interpreter's search path to locate the system config
  /// dir.
  auto
  run(const std::vector<std::string> &genomes,
      const std::string &system_config_dir,
      const bool force_download) const -> void {
    std::error_code error;
    run(genomes, system_config_dir, force_download, error);
    if (error)
      throw std::system_error(error);
  }
#endif

  /// Validate that information already assigned to member variables
  /// of this object make sense, and use defaults for those variables
  /// that must have values in order to complete the configuration
  /// process.
  [[nodiscard]] auto
  validate(std::error_code &error) noexcept -> bool;

#ifndef TRANSFERASE_NOEXCEPT
  // Overload of run that throw system_error exceptions to serve in
  /// an API.
  [[nodiscard]] auto
  validate() -> bool {
    std::error_code error;
    const auto validated = validate(error);
    if (error)
      throw std::system_error(error);
    return validated;
  }
#endif

  /// Write the client configuration file to the filesystem.
  [[nodiscard]] auto
  write() const -> std::error_code;

  /// Create directories needed for the client configuration.
  [[nodiscard]] auto
  make_directories() const -> std::error_code;

  auto
  init_config_dir(const std::string &s, std::error_code &error) -> void;

  auto
  init_index_dir(const std::string &s, std::error_code &error) -> void;

  auto
  init_labels_dir(const std::string &s, std::error_code &error) -> void;

  auto
  init_log_file(const std::string &s, std::error_code &error) -> void;

  // ADS: need to replace this with functions that take actual config
  // dir, since the config file in that dir might point to a different
  // location for the index dir.
  [[nodiscard]] static auto
  get_index_dir_default(std::error_code &error) -> std::string;

  [[nodiscard]] auto
  set_defaults(const bool force = false) -> std::error_code;

  [[nodiscard]] static auto
  get_config_dir_default(std::error_code &error) -> std::string;

  [[nodiscard]] static auto
  get_labels_dir_default(std::error_code &error) -> std::string;

  [[nodiscard]] static auto
  get_config_file_default(std::error_code &error) -> std::string;

  [[nodiscard]] auto
  tostring() const -> std::string;

private:
  auto
  init_config_file(const std::string &s, std::error_code &error) -> void;

  [[nodiscard]] auto
  get_file_default_impl(const std::string &filename,
                        std::error_code &error) -> std::string;

  [[nodiscard]] auto
  get_dir_default_impl(const std::string &dirname,
                       std::error_code &error) -> std::string;
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
)
)
// clang-format on

}  // namespace transferase

/// @brief Enum for error codes related to client configuration
enum class client_config_error_code : std::uint8_t {
  ok = 0,
  download_error = 1,
  error_creating_directories = 2,
  error_writing_config_file = 3,
  error_identifying_remote_resources = 4,
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
    case 1: return "download error"s;
    case 2: return "error creating directories"s;
    case 3: return "error writing config file"s;
    case 4: return "error idenidentifying remote resources"s;
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
