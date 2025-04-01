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

#ifndef LIB_MACOS_HELPER_HPP_
#define LIB_MACOS_HELPER_HPP_

// ADS: this file exists because in 2025, Apple Clang doesn't yet fully
// support even C++17

#include <format>
#include <string>
#include <charconv>

namespace transferase {

[[nodiscard]] static inline auto
from_chars(const char *first, const char *last,
           double &value) -> std::from_chars_result {
  // ADS: should be same as return std::from_chars(first, last, value);
  char *last_ptr{};
  value = strtod(first, &last_ptr);
  std::from_chars_result res;
  res.ptr = last_ptr;
  res.ec = (last_ptr == first || (last_ptr != last && std::isgraph(*last_ptr)))
             ? std::errc::invalid_argument
           : errno ? std::errc(errno)
                   : std::errc{};
  return res;
}

}  // namespace transferase

[[nodiscard]] inline auto
join_with(const auto &tokens, const char delim) -> std::string {
  std::string joined;
  bool first = true;
  for (const auto &token : tokens) {
    if (!first)
      joined += delim;
    joined += token;
    first = false;
  }
  return joined;
}


#endif  // LIB_MACOS_HELPER_HPP_
