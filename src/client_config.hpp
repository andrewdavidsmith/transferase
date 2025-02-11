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

/**
 * (ADS: notes to self here) client_config does the work of
 * configuring and setting up the apps, along with managing the
 * configuration of methylome_client objects. This work involves
 * writing configuration files for clients, creating directories,
 * downloading metadata and genome index files from servers, and
 * ensuring that all the right files are in the right places (or
 * noticing when they are not).
 *
 * The client config objects are intended to be used as singletons,
 * although they are not constrained as such. Their instance variables
 * can be just strings, and they can contain more information than is
 * needed. They are not performance-critical.
 *
 * Each methylome client contains a client config object. This is to
 * isolate all the code that interacts with the user's
 * configuration. Even if the user interfaces with a methylome client,
 * the work on configuration files and the configuration directories
 * is all done through a client config object.
 *
 * Responsibilities:
 *
 * - Read configuration files or report that they are not as expected.
 * - Write or update configuration files (save an instantiated object).
 * - Ensure directory structures specified in configuration exist with
 *   correct permissions (not part of interface).
 * - Make calls to download files as requested by users.
 *
 * Activities that define the interface:
 *
 * - Independent of methylome client objects:
 *   - Initial setup of transferase for a user (implemented).
 *   - Updating configuration based on user requests (not fully implemented).
 * - For methylome client objects:
 *   - Just give access to the configuration parameters through public
 *     member variables.
 */

#ifndef SRC_CLIENT_CONFIG_HPP_
#define SRC_CLIENT_CONFIG_HPP_

// ADS: Not sure why logger is needed below, but it might be that the
// full logger needs to be seen for 'mp11' and 'describe'
#include "download_policy.hpp"  // IWYU pragma: keep
#include "logger.hpp"           // IWYU pragma: keep

#include <boost/describe.hpp>

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <utility>      // for std::to_underlying, std::unreachable
#include <vector>       // IWYU pragma: keep

namespace transferase {

struct client_config {
  static constexpr auto transferase_config_dirname_default =
    ".config/transferase";
  static constexpr auto metadata_filename_default = "metadata.json";
  static constexpr auto index_dirname_default = "indexes";
  static constexpr auto client_config_filename_default =
    "transferase_client_config.conf";
  static constexpr auto client_log_filename_default = "transferase.log";

  std::string hostname;
  std::string port;
  std::string index_dir;
  std::string metadata_file;
  std::string methylome_dir;
  std::string log_file;
  log_level_t log_level{};

  [[nodiscard]] auto
  tostring() const -> std::string;

  /// Read the client configuration.
  [[nodiscard]] static auto
  read(std::string config_dir,
       std::error_code &error) noexcept -> client_config;

#ifndef TRANSFERASE_NOEXCEPT
  /// Overload of the read function that throws system_error exceptions to
  /// serve in an API
  [[nodiscard]] static auto
  read(const std::string &config_dir) -> client_config {
    std::error_code error;
    const auto obj = read(config_dir, error);
    if (error)
      throw std::system_error(error);
    return obj;
  }
#endif

  /// The `configure` function performs all the steps of setup for a
  /// client, making directories, writing configuration files, doing
  /// downloads and verifying consistency.
  auto
  configure(const std::vector<std::string> &genomes,
            const download_policy_t download_policy, std::string config_dir,
            std::string sys_config_dir,
            std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  /// Overload of `configure` for APIs. Throws system_error if an
  /// error is encountered.
  auto
  configure(const std::vector<std::string> &genomes,
            const download_policy_t download_policy,
            const std::string &config_dir,
            const std::string &sys_config_dir) const -> void {
    std::error_code error;
    configure(genomes, download_policy, config_dir, sys_config_dir, error);
    if (error)
      throw std::system_error(error);
  }
#endif

  // The set_defaults functions have overloads that take a directory
  // to look for the system config file since different APIs can't be
  // expected to find things the same way.
  auto
  set_defaults(std::string config_dir, const std::string &system_config,
               std::error_code &error) noexcept -> void;

  auto
  set_defaults(std::string config_dir, std::error_code &error) noexcept -> void;

  /// Validate that the required instance variables are set properly;
  /// do this before attempting the configuration process.
  [[nodiscard]] auto
  validate(std::error_code &error) const noexcept -> bool;

  /// Write the client configuration to the given configuration
  /// directory.
  auto
  save(std::string config_dir, std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  /// Write the client configuration to the given configuration
  /// directory, throwing system_error to serve in in an API.
  auto
  save(const std::string &config_dir) const -> void {
    std::error_code error;
    save(config_dir, error);
    if (error)
      throw std::system_error(error);
  }
#endif

  auto
  make_directories(const std::string &config_dir_arg,
                   std::error_code &error) const noexcept -> void;

  [[nodiscard]] static auto
  get_default_index_dir(std::error_code &error) -> std::string;

  [[nodiscard]] static auto
  get_default_config_dir(std::error_code &error) -> std::string;

  [[nodiscard]] static auto
  get_config_file(const std::string &config_dir,
                  std::error_code &error) -> std::string;
};

// clang-format off
BOOST_DESCRIBE_STRUCT(client_config, (), (
  hostname,
  port,
  index_dir,
  metadata_file,
  methylome_dir,
  log_file,
  log_level
)
)
// clang-format on

[[nodiscard]] auto
get_default_sys_config_dir(std::error_code &error) noexcept -> std::string;

}  // namespace transferase

/// @brief Enum for error codes related to client configuration
enum class client_config_error_code : std::uint8_t {
  ok = 0,
  metadata_download_error = 1,
  genome_index_download_error = 2,
  error_creating_directories = 3,
  error_writing_config_file = 4,
  error_identifying_remote_resources = 5,
  error_identifying_transferase_server = 6,
  invalid_client_config_information = 7,
  error_obtaining_sytem_config_dir = 8,
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
    case 1: return "metadata download error"s;
    case 2: return "genome index download error"s;
    case 3: return "error creating directories"s;
    case 4: return "error writing config file"s;
    case 5: return "error identifying remote resources"s;
    case 6: return "error identifying transferase server"s;
    case 7: return "invalid client config information"s;
    case 8: return "error obtaining system config dir"s;
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
