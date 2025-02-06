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

#include <array>  // for std::array
#include <cstdint>
#include <format>  // for std::vector??
#include <istream>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>       // for std::formatter??
#include <type_traits>  // for std::true_type
#include <utility>      // for std::to_underlying, std::unreachable
#include <vector>       // IWYU pragma: keep

namespace transferase {

enum class download_policy_t : std::uint8_t {
  none = 0,
  missing = 1,
  update = 2,
  all = 3,
};

// clang-format off
BOOST_DESCRIBE_ENUM(
  download_policy_t,
  none,
  missing,
  update,
  all
)
// clang-format on

static constexpr auto download_policy_t_name = std::array{
  // clang-format off
  std::string_view{"none"},
  std::string_view{"missing"},
  std::string_view{"update"},
  std::string_view{"all"},
  // clang-format on
};

inline auto
operator<<(std::ostream &o, const download_policy_t &dp) -> std::ostream & {
  return o << download_policy_t_name[std::to_underlying(dp)];
}

inline auto
operator>>(std::istream &in, download_policy_t &dp) -> std::istream & {
  std::string tmp;
  if (!(in >> tmp))
    return in;
  for (const auto [idx, name] : std::views::enumerate(level_name))
    if (tmp == name) {
      dp = static_cast<download_policy_t>(idx);
      return in;
    }
  in.setstate(std::ios::failbit);
  return in;
}

}  // namespace transferase

template <>
struct std::formatter<transferase::download_policy_t>
  : std::formatter<std::string> {
  auto
  format(const transferase::download_policy_t &of,
         std::format_context &ctx) const {
    return std::format_to(
      ctx.out(), "{}",
      transferase::download_policy_t_name[std::to_underlying(of)]);
  }
};

namespace transferase {

struct client_config {
  static constexpr auto transferase_config_dirname_default =
    ".config/transferase";
  static constexpr auto metadata_filename_default = "metadata.json";
  static constexpr auto index_dirname_default = "indexes";
  static constexpr auto client_config_filename_default =
    "transferase_client_config.toml";
  static constexpr auto client_log_filename_default = "transferase.log";

  std::string hostname;
  std::string port;
  std::string index_dir;
  std::string metadata_file;
  std::string methylome_dir;
  std::string log_file;
  log_level_t log_level{};

  /// Read the client configuration.
  [[nodiscard]] static auto
  read(const std::string &config_dir,
       std::error_code &error) noexcept -> client_config;

#ifndef TRANSFERASE_NOEXCEPT
  /// Overload of the read function that throws system_error exceptions to serve
  /// in an API
  [[nodiscard]] static auto
  read(const std::string &config_dir) -> client_config {
    std::error_code error;
    const auto obj = read(config_dir, error);
    if (error)
      throw std::system_error(error);
    return obj;
  }
#endif

  /// Read the client configuration.
  [[nodiscard]] static auto
  read(std::error_code &error) noexcept -> client_config;

#ifndef TRANSFERASE_NOEXCEPT
  /// Overload of the read function that throws system_error exceptions to serve
  /// in an API
  [[nodiscard]] static auto
  read() -> client_config {
    std::error_code error;
    const auto obj = read(error);
    if (error)
      throw std::system_error(error);
    return obj;
  }
#endif

  /// These are different overloads of the `configure` function that
  /// performs the step of making directories, writing configuration
  /// files and doing downloads. In other words, 'configure' does the
  /// work of configuring the client.
  ///
  /// Each overload of the `configure` function handles a different
  /// combination of argument types, and the functions with fewer
  /// arguments use deduced system defaults for the values that are
  /// not specified. Each overload has a throwing and non-throwing
  /// version. The no-throwing version uses an outparam error code and
  /// does the work, while the throwing version simply converts the
  /// error code into an exception. The throwing versions are for
  /// APIs.

  /// Set the client configuration specifying all possible values.
  /// This overload accepts a system configuration directory, which is
  /// essential in APIs for interpreted languages.
  auto
  configure(const std::string &config_dir,
            const std::vector<std::string> &genomes,
            const std::string &system_config_dir,
            const download_policy_t download_policy,
            std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  /// Set the client configuration specifying all possible values,
  /// throwing system_error if any error is encountered.  This
  /// overload accepts a system configuration directory, which is
  /// essential in APIs for interpreted languages.
  auto
  configure(const std::string &config_dir,
            const std::vector<std::string> &genomes,
            const std::string &system_config_dir,
            const download_policy_t download_policy) const -> void {
    std::error_code error;
    configure(config_dir, genomes, system_config_dir, download_policy, error);
    if (error)
      throw std::system_error(error);
  }
#endif

  /// Set the client configuration specifying all possible values
  /// except the system configuration directory, which is deduced
  /// using the path the current process' binary. This overload
  /// accepts a user-specified configuration directory if the user
  /// does not want to use the default location.
  auto
  configure(const std::string &config_dir,
            const std::vector<std::string> &genomes,
            const download_policy_t download_policy,
            std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  /// Set the client configuration specifying all possible values
  /// except the system configuration directory, throwing system_error
  /// if any error is encountered.  The configuration directory is
  /// deduced using the path the current process' binary. This
  /// overload accepts a user-specified configuration directory if the
  /// user does not want to use the default location.
  auto
  configure(const std::string &config_dir,
            const std::vector<std::string> &genomes,
            const download_policy_t download_policy) const -> void {
    std::error_code error;
    configure(config_dir, genomes, download_policy, error);
    if (error)
      throw std::system_error(error);
  }
#endif

  /// Configure, accepting a system configuration directory for use in
  /// APIs.
  auto
  configure(const std::vector<std::string> &genomes,
            const std::string &system_config_dir,
            const download_policy_t download_policy,
            std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  auto
  configure(const std::vector<std::string> &genomes,
            const std::string &system_config_dir,
            const download_policy_t download_policy) const -> void {
    std::error_code error;
    configure(genomes, system_config_dir, download_policy, error);
    if (error)
      throw std::system_error(error);
  }
#endif

  auto
  configure(const std::vector<std::string> &genomes,
            const download_policy_t download_policy,
            std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  auto
  configure(const std::vector<std::string> &genomes,
            const download_policy_t download_policy) const -> void {
    std::error_code error;
    configure(genomes, download_policy, error);
    if (error)
      throw std::system_error(error);
  }
#endif

  // The set_defaults functions have overloads that take a directory
  // to look for the system config file since different APIs can't be
  // expected to find things the same way.

  auto
  set_defaults_system_config(const std::string &config_dir,
                             const std::string &system_config,
                             std::error_code &error) noexcept -> void;

  auto
  set_defaults_system_config(const std::string &system_config,
                             std::error_code &error) noexcept -> void;

  auto
  set_defaults(const std::string &config_dir,
               std::error_code &error) noexcept -> void;

  auto
  set_defaults(std::error_code &error) noexcept -> void;

  // ADS: need to replace this with functions that take actual config
  // dir, since the config file in that dir might point to a different
  // location for the index dir.
  [[nodiscard]] static auto
  get_index_dir_default(std::error_code &error) -> std::string;

  [[nodiscard]] static auto
  get_config_dir_default(std::error_code &error) -> std::string;

  [[nodiscard]] static auto
  get_metadata_file_default(std::error_code &error) -> std::string;

  [[nodiscard]] static auto
  get_config_file(const std::string &config_dir,
                  std::error_code &error) -> std::string;

  [[nodiscard]] auto
  tostring() const -> std::string;

  /// Validate that the required instance variables are set properly;
  /// do this before attempting the configuration process.
  [[nodiscard]] auto
  validate(std::error_code &error) const noexcept -> bool;

  /// Create directories needed for the client configuration.
  [[nodiscard]] auto
  make_directories(std::string config_dir) const -> std::error_code;

  /// Write the client configuration to the default configuration
  /// directory.
  auto
  write(std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  /// Write the client configuration to the default configuration
  /// directory, throwing system_error to serve in in an API.
  auto
  write() const -> void {
    std::error_code error;
    write(error);
    if (error)
      throw std::system_error(error);
  }
#endif

  /// Write the client configuration to the given configuration
  /// directory.
  auto
  write(const std::string &config_dir,
        std::error_code &error) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  /// Write the client configuration to the given configuration
  /// directory, throwing system_error to serve in in an API.
  auto
  write(const std::string &config_dir) const -> void {
    std::error_code error;
    write(config_dir, error);
    if (error)
      throw std::system_error(error);
  }
#endif

private:
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
  metadata_file,
  methylome_dir,
  log_file,
  log_level
)
)
// clang-format on

[[nodiscard]] auto
get_default_system_config_dir(std::error_code &error) noexcept -> std::string;

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
