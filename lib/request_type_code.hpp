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

#ifndef LIB_REQUEST_TYPE_CODE_HPP_
#define LIB_REQUEST_TYPE_CODE_HPP_

#include <array>
#include <cstdint>  // for std::uint32_t
#include <format>
#include <iterator>
#include <string>
#include <utility>  // for to_underlying, unreachable

namespace transferase {

enum class request_type_code : std::uint8_t {
  intervals = 0,
  intervals_covered = 1,
  bins = 2,
  bins_covered = 3,
};

static constexpr auto request_type_code_names = std::array{
  // clang-format off
  "intervals",
  "intervals_covered",
  "bins",
  "bins_covered",
  // clang-format on
};

static constexpr std::underlying_type_t<request_type_code> n_xfr_request_types =
  std::size(request_type_code_names);

[[nodiscard]] inline auto
to_string(const request_type_code c) -> std::string {
  return std::format("{}", std::to_underlying(c));
}

}  // namespace transferase

/// Helper to specialize std::format for request_type_code
template <>
struct std::formatter<transferase::request_type_code>
  : std::formatter<std::string> {
  /// Specialization of std::format for request_type_code
  auto
  format(const transferase::request_type_code &rtc, auto &ctx) const {
    return std::formatter<std::string>::format(
      std::format("{}", std::to_underlying(rtc)), ctx);
  }
};

#endif  // LIB_REQUEST_TYPE_CODE_HPP_
