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

#ifndef SRC_COUNTS_FILE_FORMATS_HPP_
#define SRC_COUNTS_FILE_FORMATS_HPP_

// ADS: code here is to handle the different counts file formats used
// by dnmtools and others, including determining format, validating
// format and parsing lines.

#include <cstdint>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>  // std::unreachable

namespace xfrase {

enum class counts_format : std::uint32_t {
  none = 0,
  xcounts = 1,
  counts = 2,
};

[[nodiscard]] inline auto
message(const counts_format format_code) -> std::string {
  using std::string_literals::operator""s;
  // clang-format off
  switch (format_code) {
  case counts_format::none: return "not known"s;
  case counts_format::xcounts: return "xcounts"s;
  case counts_format::counts: return "counts"s;
  }
  // clang-format on
  std::unreachable();  // hopefully this is unreacheable
}

[[nodiscard]] auto
parse_counts_line(const std::string &line, std::uint32_t &pos,
                  std::uint32_t &n_meth, uint32_t &n_unmeth) -> bool;

[[nodiscard]] auto
get_meth_file_format(const std::string &filename)
  -> std::tuple<counts_format, std::error_code>;

}  // namespace xfrase

#endif  // SRC_COUNTS_FILE_FORMATS_HPP_
