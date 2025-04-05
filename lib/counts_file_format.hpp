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

#ifndef LIB_COUNTS_FILE_FORMAT_HPP_
#define LIB_COUNTS_FILE_FORMAT_HPP_

// ADS: code here is to handle the different counts file formats used
// by dnmtools and others, including determining format, validating
// format and parsing lines.

#include <array>
#include <cstdint>
#include <format>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>  // std::unreachable
#include <variant>  // for std::tuple

namespace transferase {

enum class counts_file_format : std::uint8_t {
  unknown = 0,
  xcounts = 1,
  counts = 2,
};

using std::literals::string_literals::operator""s;
static const auto counts_file_format_name = std::array{
  // clang-format off
  "unknown"s,
  "xcounts"s,
  "counts"s,
  // clang-format on
};

[[nodiscard]] auto
parse_counts_line(const std::string &line, std::uint32_t &pos,
                  std::uint32_t &n_meth, uint32_t &n_unmeth) -> bool;

[[nodiscard]] auto
get_meth_file_format(const std::string &filename)
  -> std::tuple<counts_file_format, std::error_code>;

}  // namespace transferase

template <>
struct std::formatter<transferase::counts_file_format>
  : std::formatter<std::string> {
  auto
  format(const transferase::counts_file_format &x, auto &ctx) const {
    const auto u = std::to_underlying(x);
    const auto v = transferase::counts_file_format_name[u];
    return std::formatter<std::string>::format(v, ctx);
  }
};

#endif  // LIB_COUNTS_FILE_FORMAT_HPP_
