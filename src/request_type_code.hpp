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

#ifndef SRC_REQUEST_TYPE_CODE_HPP_
#define SRC_REQUEST_TYPE_CODE_HPP_

#include <array>
#include <cstdint>  // for std::uint32_t
#include <format>
#include <string>
#include <string_view>
#include <utility>  // for to_underlying, unreachable

namespace xfrase {

enum class request_type_code : std::uint8_t {
  intervals = 0,
  intervals_covered = 1,
  bins = 2,
  bins_covered = 3,
  unknown = 4,
  n_request_types = 5,
};

static constexpr auto request_type_code_names = std::array{
  // clang-format off
  "intervals",
  "intervals_covered",
  "bins",
  "bins_covered",
  "unknown",
  // clang-format on
};

}  // namespace xfrase

template <>
struct std::formatter<xfrase::request_type_code> : std::formatter<std::string> {
  auto
  format(const xfrase::request_type_code &rtc, std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}", std::to_underlying(rtc));
  }
};

#endif  // SRC_REQUEST_TYPE_CODE_HPP_
