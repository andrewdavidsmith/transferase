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

#ifndef LIB_LEVEL_ELEMENT_FORMATTER_HPP_
#define LIB_LEVEL_ELEMENT_FORMATTER_HPP_

#include "level_element.hpp"

#include <charconv>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace transferase {

enum class level_element_mode : std::uint8_t {
  counts,
  classic,
  score,
};

static inline auto
push_buffer(auto &a, const auto b, auto &e, const std::uint32_t val) -> void {
  auto tcr = std::to_chars(a, b, val);
  e = e != std::errc{} ? e : tcr.ec;
  a = tcr.ptr;
}

static inline auto
push_buffer(auto &a, const auto b, auto &e,
            const std::floating_point auto val) -> void {
  auto tcr = std::to_chars(a, b, val, std::chars_format::general, 6);
  e = e != std::errc{} ? e : tcr.ec;
  a = tcr.ptr;
}

static inline auto
push_buffer(auto &a, const auto b, [[maybe_unused]] auto &e,
            const std::string_view val) -> void {
  auto v = std::cbegin(val);
  while (a != b && v != std::cend(val))
    *a++ = *v++;
  e = (e == std::errc{} && a == b) ? std::errc::result_out_of_range : e;
}

static inline auto
push_buffer(auto &a, const auto b, [[maybe_unused]] auto &e,
            const std::string &val) -> void {
  auto v = std::cbegin(val);
  while (a != b && v != std::cend(val))
    *a++ = *v++;
  e = (e == std::errc{} && a == b) ? std::errc::result_out_of_range : e;
}

static inline auto
push_buffer(auto &a, const auto b, [[maybe_unused]] auto &e,
            const char val) -> void {
  if (a != b)
    *a++ = val;
}

template <typename... Args>
inline auto
push_buffer(auto &a, const auto b, auto &e, Args... args) -> void {
  (push_buffer(a, b, e, args), ...);
}

inline auto
push_buffer_elem(auto &a, const auto b, auto &e, const level_element_t &elem,
                 const level_element_mode mode, const char delim) -> void {
  switch (mode) {
  case level_element_mode::counts:
    push_buffer(a, b, e, delim, elem.n_meth, delim, elem.n_unmeth);
    break;
  case level_element_mode::classic:
    push_buffer(a, b, e, delim, elem.get_wmean(), delim, elem.n_reads());
    break;
  case level_element_mode::score:
    push_buffer(a, b, e, delim, elem.get_wmean());
    break;
  }
}

inline auto
push_buffer_elem(auto &a, const auto b, auto &e,
                 const level_element_covered_t &elem,
                 const level_element_mode mode, const char delim) -> void {
  switch (mode) {
  case level_element_mode::counts:
    push_buffer(a, b, e, delim, elem.n_meth, delim, elem.n_unmeth, delim,
                elem.n_covered);
    break;
  case level_element_mode::classic:
    push_buffer(a, b, e, delim, elem.get_wmean(), delim, elem.n_reads(), delim,
                elem.n_covered);
    break;
  case level_element_mode::score:
    push_buffer(a, b, e, delim, elem.get_wmean());
    break;
  }
}

inline auto
push_buffer_score(auto &a, const auto b, auto &e, const auto &elem,
                  const std::string_view none_label,
                  const std::uint32_t min_reads, const char delim) -> void {
  if (elem.n_reads() >= min_reads)
    push_buffer(a, b, e, delim, elem.get_wmean());
  else
    push_buffer(a, b, e, delim, none_label);
}

}  // namespace transferase

#endif  // LIB_LEVEL_ELEMENT_FORMATTER_HPP_
