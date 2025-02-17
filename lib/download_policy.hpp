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

#ifndef LIB_DOWNLOAD_POLICY_HPP_
#define LIB_DOWNLOAD_POLICY_HPP_

#include <array>
#include <cstdint>
#include <format>
#include <iostream>
#include <map>
#include <string>
#include <string_view>

namespace transferase {

enum class download_policy_t : std::uint8_t {
  none = 0,
  missing = 1,
  update = 2,
  all = 3,
};

static constexpr auto download_policy_t_name = std::array{
  // clang-format off
  std::string_view{"none"},
  std::string_view{"missing"},
  std::string_view{"update"},
  std::string_view{"all"},
  // clang-format on
};

static const std::map<std::string, download_policy_t> download_policy_cli11{
  // clang-format off
  {"none", download_policy_t::none},
  {"missing", download_policy_t::missing},
  {"update", download_policy_t::update},
  {"all", download_policy_t::all},
  // clang-format on
};

auto
operator<<(std::ostream &o, const download_policy_t &dp) -> std::ostream &;

auto
operator>>(std::istream &in, download_policy_t &dp) -> std::istream &;

}  // namespace transferase

template <>
struct std::formatter<transferase::download_policy_t>
  : std::formatter<std::string> {
  auto
  format(const transferase::download_policy_t &dp,
         std::format_context &ctx) const -> std::format_context::iterator;
};

#endif  // LIB_DOWNLOAD_POLICY_HPP_
