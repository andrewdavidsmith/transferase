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

#ifndef LIB_TRANSFER_STATS_HPP_
#define LIB_TRANSFER_STATS_HPP_

#include <algorithm>
#include <cstdint>
#include <format>
#include <limits>
#include <string>

struct transfer_stats {
  std::uint32_t n_xfrs{};
  std::uint32_t xfr_bytes{};
  std::uint32_t min_xfr_size{std::numeric_limits<std::uint32_t>::max()};
  std::uint32_t max_xfr_size{};

  auto
  update(const std::uint32_t n_bytes) -> void {
    // Don't count zero-byte reads
    if (n_bytes == 0)
      return;
    ++n_xfrs;
    const auto delta_bytes = n_bytes - xfr_bytes;
    xfr_bytes = n_bytes;
    max_xfr_size = std::max(max_xfr_size, delta_bytes);
    min_xfr_size = std::min(min_xfr_size, delta_bytes);
  }

  [[nodiscard]] auto
  str() const -> std::string {
    static constexpr auto fmt = "{}B, N={}, max={}B, min={}B";
    return std::format(fmt, xfr_bytes, n_xfrs, max_xfr_size, min_xfr_size);
  }
};

#endif  // LIB_TRANSFER_STATS_HPP_
