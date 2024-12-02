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

/*
  Functions defined here are used by multiple source files
 */

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// getpwuid_r
#include <pwd.h>
#include <sys/types.h>

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
get_mxe_config_dir_default(std::error_code &ec) -> std::string {
  static const auto config_dir_rhs = std::filesystem::path(".config/mxe");
  static const auto env_home = std::getenv("HOME");
  if (!env_home) {
    ec = std::make_error_code(std::errc{errno});
    return {};
  }
  const std::filesystem::path config_dir = env_home / config_dir_rhs;
  return config_dir;
}

/*
auto
check_mxe_config_dir(const std::string &dirname, std::error_code &ec) -> bool
{ const bool exists = std::filesystem::exists(dirname, ec); if (ec) return
false;

  if (!exists) {
    ec = std::make_error_code(std::errc::no_such_file_or_directory);
    return false;
  }

  const bool is_dir = std::filesystem::is_directory(dirname, ec);
  if (ec)
    return false;
  if (!is_dir) {
    ec = std::make_error_code(std::errc::not_a_directory);
    return false;
  }

  return true;
}
*/
