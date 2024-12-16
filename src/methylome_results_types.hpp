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

#ifndef SRC_METHYLOME_RESULTS_TYPES_HPP_
#define SRC_METHYLOME_RESULTS_TYPES_HPP_

#include <cstdint>
#include <format>
#include <string>

struct counts_res {
  std::uint32_t n_meth{};
  std::uint32_t n_unmeth{};
};

struct counts_res_cov {
  std::uint32_t n_meth{};
  std::uint32_t n_unmeth{};
  std::uint32_t n_covered{};
};

template <> struct std::formatter<counts_res> : std::formatter<std::string> {
  auto
  format(const counts_res &cr, std::format_context &ctx) const {
    return std::format_to(ctx.out(), R"({{"n_meth": {}, "n_unmeth": {}}})",
                          cr.n_meth, cr.n_unmeth);
  }
};

template <>
struct std::formatter<counts_res_cov> : std::formatter<std::string> {
  auto
  format(const counts_res_cov &cr, std::format_context &ctx) const {
    return std::format_to(
      ctx.out(), R"({{"n_meth": {}, "n_unmeth": {}, "n_covered": {}}})",
      cr.n_meth, cr.n_unmeth, cr.n_covered);
  }
};

#endif  // SRC_METHYLOME_RESULTS_TYPES_HPP_
