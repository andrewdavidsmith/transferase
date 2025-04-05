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

#ifndef LIB_OUTPUT_FORMAT_TYPE_HPP_
#define LIB_OUTPUT_FORMAT_TYPE_HPP_

#include <array>
#include <cstdint>
#include <format>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <utility>  // for std::to_underlying

namespace transferase {

enum class output_format_t : std::uint8_t {
  none = 0,
  counts = 1,
  bedgraph = 2,
  dataframe = 3,
  dfscores = 4,
  classic = 5,
  dfclassic = 6,
};

static constexpr auto output_format_t_name = std::array{
  // clang-format off
  std::string_view{"none"},
  std::string_view{"counts"},
  std::string_view{"bedgraph"},
  std::string_view{"dataframe"},
  std::string_view{"dfscores"},
  std::string_view{"classic"},
  std::string_view{"dfclassic"},
  // clang-format on
};

static const std::map<std::string, output_format_t> output_format_cli11{
  // clang-format off
  {"none", output_format_t::none},
  {"counts", output_format_t::counts},
  {"bedgraph", output_format_t::bedgraph},
  {"dataframe", output_format_t::dataframe},
  {"dfscores", output_format_t::dfscores},
  {"classic", output_format_t::classic},
  {"dfclassic", output_format_t::dfclassic},
  // clang-format on
};

auto
operator<<(std::ostream &o, const output_format_t &of) -> std::ostream &;

auto
operator>>(std::istream &in, output_format_t &of) -> std::istream &;

}  // namespace transferase

template <>
struct std::formatter<transferase::output_format_t>
  : std::formatter<std::string> {
  auto
  format(const transferase::output_format_t &x, auto &ctx) const {
    const auto u = std::to_underlying(x);
    const auto v = transferase::output_format_t_name[u];
    return std::formatter<std::string>::format(
      std::string(std::cbegin(v), std::cend(v)), ctx);
  }
};

#endif  // LIB_OUTPUT_FORMAT_TYPE_HPP_
