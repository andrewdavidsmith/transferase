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

#include "download_policy.hpp"

#include <array>
#include <iostream>
#include <ranges>  // IWYU pragma: keep
#include <string>
#include <string_view>
#include <tuple>    // IWYU pragma: keep
#include <utility>  // for std::to_underlying

namespace transferase {

auto
operator<<(std::ostream &o, const download_policy_t &dp) -> std::ostream & {
  // NOLINTNEXTLINE(*-array-index)
  return o << download_policy_t_name[std::to_underlying(dp)];
}

auto
operator>>(std::istream &in, download_policy_t &dp) -> std::istream & {
  std::string tmp;
  if (!(in >> tmp))
    return in;
  // for (const auto [idx, name] :
  // std::views::enumerate(download_policy_t_name))
  std::uint32_t idx = 0;
  for (const auto name : download_policy_t_name) {
    if (tmp == name) {
      dp = static_cast<download_policy_t>(idx);
      return in;
    }
    ++idx;
  }
  in.setstate(std::ios::failbit);
  return in;
}

}  // namespace transferase
