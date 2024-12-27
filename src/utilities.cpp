/* MIT License
 *
 * Copyright (c) 2024 Andrew D Smith
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

#include "utilities.hpp"

#include "logger.hpp"        // IWYU pragma: keep
#include "xfrase_error.hpp"  // IWYU pragma: keep

#include <array>
#include <cerrno>
#include <cstdlib>  // for getenv
#include <fstream>
#include <string>
#include <tuple>
#include <utility>  // for std::move

// getpwuid_r
#include <pwd.h>
#include <unistd.h>  // for getuid

[[nodiscard]] auto
get_xfrase_config_dir_default(std::error_code &ec) -> std::string {
  static const auto config_dir_rhs = std::filesystem::path(".config/xfrase");
  static const auto env_home = std::getenv("HOME");
  if (!env_home) {
    ec = std::make_error_code(std::errc{errno});
    return {};
  }
  const std::filesystem::path config_dir = env_home / config_dir_rhs;
  return config_dir;
}

[[nodiscard]]
auto
check_output_file(const std::string &filename) -> std::error_code {
  std::error_code ec;
  // get the full name
  const auto canonical = std::filesystem::weakly_canonical(filename, ec);
  if (ec)
    return ec;

  // check if this is a directory
  const bool already_exists = std::filesystem::exists(canonical);
  if (already_exists) {
    const bool is_dir = std::filesystem::is_directory(canonical, ec);
    if (ec)
      return ec;
    if (is_dir)
      return std::error_code{output_file_error::is_a_directory};
    return {};
  }

  // if it doesn't already exist, test if it's writable
  // if (!already_exists) // ADS: would be redundant for now
  {
    std::ofstream out_test(canonical);
    if (!out_test)
      ec = output_file_error::failed_to_open;
    [[maybe_unused]] std::error_code unused{};
    const bool out_test_exits = std::filesystem::exists(canonical, unused);
    if (out_test_exits) {
      [[maybe_unused]]
      const bool removed = std::filesystem::remove(canonical, unused);
    }
    return ec;
  }
  // ADS: somehow need to check if an existing file can be written
  // without modifying it??

  return {};
}

[[nodiscard]] auto
generate_temp_filename(const std::string &prefix,
                       const std::string &suffix) -> std::string {
  const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
  const auto filename =
    std::format("{}_{}{}", prefix, millis,
                (suffix.empty() || suffix[0] == '.') ? suffix : "." + suffix);
  return (std::filesystem::temp_directory_path() / filename).string();
}
