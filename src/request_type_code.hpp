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

enum class request_type_code : std::uint32_t {
  counts = 0,
  counts_cov = 1,
  bin_counts = 2,
  bin_counts_cov = 3,
  unknown = 4,
  n_request_types = 5,
};

static constexpr auto request_type_code_names = std::array{
  // clang-format off
  "counts",
  "counts_cov",
  "bin_counts",
  "bin_counts_cov",
  "unknown",
  // clang-format on
};

template <>
struct std::formatter<request_type_code> : std::formatter<std::string> {
  auto
  format(const request_type_code &rtc, std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}", std::to_underlying(rtc));
  }
};

#endif  // SRC_REQUEST_TYPE_CODE_HPP_