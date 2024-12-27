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

#include "environment_utilities.hpp"

#include <config.h>

#include <boost/asio.hpp>    // boost::asio::ip::host_name();
#include <boost/system.hpp>  // for boost::system::error_code

// getpwuid_r
#include <pwd.h>
#include <unistd.h>  // for getuid

#include <array>
#include <cerrno>
#include <chrono>
#include <format>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>

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
get_hostname() -> std::tuple<std::string, std::error_code> {
  boost::system::error_code boost_err;
  const auto host = boost::asio::ip::host_name(boost_err);
  if (boost_err)
    return {std::string{}, std::error_code{boost_err}};
  return {host, std::error_code{}};
}

[[nodiscard]] auto
get_version() -> std::string {
  return VERSION;
}
