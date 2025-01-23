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

#include "output_format_type.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <iostream>
#include <iterator>  // for std::size
#include <ranges>
#include <string>
#include <type_traits>  // for std::underlying_type_t
#include <utility>      // for std::to_underlying

namespace transferase {

auto
operator<<(std::ostream &o,
           const transferase::output_format_t &of) -> std::ostream & {
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
  return o << transferase::output_format_t_name[std::to_underlying(of)];
  // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
}

auto
operator>>(std::istream &in,
           transferase::output_format_t &of) -> std::istream & {
  constexpr auto is_digit = [](const auto c) {
    return std::isdigit(static_cast<unsigned char>(c));
  };
  std::string tmp;
  if (!(in >> tmp))
    return in;

  if (std::ranges::all_of(tmp, is_digit)) {
    std::underlying_type_t<transferase::output_format_t> num{};
    const auto last = tmp.data() + std::size(tmp);  // NOLINT
    const auto res = std::from_chars(tmp.data(), last, num);
    if (res.ptr != last) {
      in.setstate(std::ios::failbit);
      return in;
    }
    of = static_cast<transferase::output_format_t>(num);
    return in;
  }

  for (const auto [i, name] :
       std::views::enumerate(transferase::output_format_t_name))
    if (tmp == name) {
      of = static_cast<transferase::output_format_t>(i);
      return in;
    }
  in.setstate(std::ios::failbit);
  return in;
}

}  // namespace transferase
