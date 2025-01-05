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

#ifndef SRC_LEVEL_ELEMENT_HPP_
#define SRC_LEVEL_ELEMENT_HPP_

#include <cstdint>
#include <format>
#include <string>

namespace transferase {

struct level_element_t {
  std::uint32_t n_meth{};
  std::uint32_t n_unmeth{};
  auto
  operator<=>(const level_element_t &) const = default;
};

struct level_element_covered_t {
  std::uint32_t n_meth{};
  std::uint32_t n_unmeth{};
  std::uint32_t n_covered{};
  auto
  operator<=>(const level_element_covered_t &) const = default;
};

}  // namespace transferase

template <>
struct std::formatter<transferase::level_element_t> : std::formatter<std::string> {
  auto
  format(const transferase::level_element_t &l, std::format_context &ctx) const {
    return std::format_to(ctx.out(), R"({{"n_meth": {}, "n_unmeth": {}}})",
                          l.n_meth, l.n_unmeth);
  }
};

template <>
struct std::formatter<transferase::level_element_covered_t>
  : std::formatter<std::string> {
  auto
  format(const transferase::level_element_covered_t &l,
         std::format_context &ctx) const {
    return std::format_to(
      ctx.out(), R"({{"n_meth": {}, "n_unmeth": {}, "n_covered": {}}})",
      l.n_meth, l.n_unmeth, l.n_covered);
  }
};

#endif  // SRC_LEVEL_ELEMENT_HPP_
