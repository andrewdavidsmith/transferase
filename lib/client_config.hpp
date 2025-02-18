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

#ifndef LIB_CLIENT_CONFIG_HPP_
#define LIB_CLIENT_CONFIG_HPP_

// ADS: Not sure why logger is needed below, but it might be that the
// full logger needs to be seen for 'mp11' and 'describe'
#include "download_policy.hpp"  // IWYU pragma: keep
#include "logger.hpp"           // IWYU pragma: keep
#include "nlohmann/json.hpp"    // IWYU pragma: keep
#include "transferase_metadata.hpp"

#include <cstdint>
#include <format>
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
    "transferase_client.json";
  static constexpr auto client_log_filename_default = "transferase.log";

  transferase_metadata meta;
  std::string config_dir;
  std::string hostname;
  std::uint16_t port{};
  std::string index_dir;
  std::string metadata_file;
  std::string methylome_dir;
  std::string log_file;
  log_level_t log_level{};

  // The constructor has an overload that take a directory to look for
  // the system config file since different APIs can't be expected to
  // find things the same way.
  client_config(const std::string &config_dir, const std::string &system_config,
                std::error_code &error) noexcept;

  client_config(const std::string &config_dir, std::error_code &error) noexcept;

#ifndef TRANSFERASE_NOEXCEPT
  client_config(const std::string &config_dir,
                const std::string &system_config);
#endif

  [[nodiscard]] auto
  tostring() const -> std::string;

  /// Get the path to the index directory
  [[nodiscard]] auto
  get_index_dir() const noexcept -> std::string;

  /// Get the path to the metadata file
  [[nodiscard]] auto
  get_metadata_file() const noexcept -> std::string;

  /// Get the path to the methylome directory
  [[nodiscard]] auto
  get_methylome_dir() const noexcept -> std::string;

  /// Get the path to the log file
  [[nodiscard]] auto
  get_log_file() const noexcept -> std::string;

  /// Read the transferase metadata file
  auto
  load_transferase_metadata(std::error_code &error) -> void;

  [[nodiscard]] auto
  available_genomes() const noexcept -> std::vector<std::string> {
    return meta.available_genomes();
  }

  /// Read the client configuration.
  [[nodiscard]] static auto
  read(std::string config_dir,
       std::error_code &error) noexcept -> client_config;

  /// Initialize any empty values by reading the config file
  auto
  read_config_file_no_overwrite(std::error_code &error) noexcept -> void;

  /// Initialize all values from a given config file
  [[nodiscard]] static auto
  read_config_file(const std::string &config_file,
                   std::error_code &error) noexcept -> client_config;

#ifndef TRANSFERASE_NOEXCEPT
  /// Overload of the read function that throws system_error exceptions to
  /// serve in an API
  [[nodiscard]] static auto
  read(const std::string &config_dir) -> client_config {
    std::error_code error;
    const auto obj = read(config_dir, error);
    if (error) {
      const auto message = std::format(
        "[Error reading client config (config_dir: {})]", config_dir);
      throw std::system_error(error, message);
    }
    return obj;
  }
#endif

  /// The `install` function performs the steps that involve
  /// downloads. It will also make directories, write configuration
  /// files and verifying consistency, if needed.
  auto
  install(const std::vector<std::string> &genomes,
          const download_policy_t download_policy, std::string sys_config_dir,
          std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  /// Overload of `install` for APIs. Throws system_error if an
  /// error is encountered.
  auto
  install(const std::vector<std::string> &genomes,
          const download_policy_t download_policy,
          const std::string &sys_config_dir) const -> void {
    std::error_code error;
    install(genomes, download_policy, sys_config_dir, error);
    if (error)
      throw std::system_error(error);
  }
#endif

  /// Validate that the required instance variables are set properly;
  /// do this before attempting the configuration process.
  [[nodiscard]] auto
  validate(std::error_code &error) const noexcept -> bool;

  /// Write the client configuration to the given configuration
  /// directory.
  auto
  save(std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  /// Write the client configuration to the given configuration
  /// directory, throwing system_error to serve in in an API.
  auto
  save() const -> void {
    std::error_code error;
    save(error);
    if (error)
      throw std::system_error(error);
  }
#endif

  auto
  make_paths_absolute() noexcept -> void;

  auto
  make_directories(std::error_code &error) const noexcept -> void;

  [[nodiscard]] static auto
  get_default_config_dir(std::error_code &error) -> std::string;

  /// Get the path to the config file (base on dir)
  [[nodiscard]] static auto
  get_config_file(const std::string &config_dir) noexcept -> std::string;

  [[nodiscard]] static auto
  get_config_file(const std::string &config_dir,
                  std::error_code &error) -> std::string;

  client_config() = default;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(client_config, config_dir, hostname, port,
                                 index_dir, metadata_file, methylome_dir,
                                 log_file, log_level)
};

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
  failed_to_read_transferase_metadata_file = 9,
  failed_to_read_client_config_file = 10,
  failed_to_parse_client_config_file = 11,
  invalid_client_config_file = 12,
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
    case 9: return "failed to read transferase metadata file"s;
    case 10: return "failed to read client config file"s;
    case 11: return "failed to parse client config file"s;
    case 12: return "invalid client config file"s;
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

#endif  // LIB_CLIENT_CONFIG_HPP_
