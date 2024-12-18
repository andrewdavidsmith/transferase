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
get_username() -> std::tuple<std::string, std::error_code> {
  // ADS: long code here; static needs it for threadsafe
  constexpr auto bufsize{1024};
  struct passwd pwd;
  struct passwd *result;
  std::array<char, bufsize> buf;
  const auto s = getpwuid_r(getuid(), &pwd, buf.data(), bufsize, &result);
  if (result == nullptr)
    return {{},
            std::make_error_code(s == 0 ? std::errc::invalid_argument
                                        : std::errc(errno))};
  // ADS: pw_name below might be better as pw_gecos
  return {std::move(std::string(pwd.pw_name)), {}};
}

[[nodiscard]] auto
get_time_as_string() -> std::string {
  const auto now{std::chrono::system_clock::now()};
  const std::chrono::year_month_day ymd{
    std::chrono::floor<std::chrono::days>(now)};
  const std::chrono::hh_mm_ss hms{
    std::chrono::floor<std::chrono::seconds>(now) -
    std::chrono::floor<std::chrono::days>(now)};
  return std::format("{:%F} {:%T}", ymd, hms);
}

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
  const bool is_dir = std::filesystem::is_directory(canonical, ec);
  if (ec)
    return ec;

  if (is_dir)
    return std::error_code{output_file_error::is_a_directory};

  // if it doesn't already exist, test if it's writable
  if (!std::filesystem::exists(canonical)) {
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
