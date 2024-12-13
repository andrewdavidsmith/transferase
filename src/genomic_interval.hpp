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

#ifndef SRC_GENOMIC_INTERVAL_HPP_
#define SRC_GENOMIC_INTERVAL_HPP_

#include <cstdint>
#include <format>
#include <string>
#include <system_error>
#include <vector>

struct cpg_index_meta;
struct genomic_interval_load_ret;

struct genomic_interval {
  std::int32_t ch_id{-1};
  std::uint32_t start{};
  std::uint32_t stop{};

  auto
  operator<=>(const genomic_interval &) const = default;

  [[nodiscard]] static auto
  load(const cpg_index_meta &index,
       const std::string &filename) -> genomic_interval_load_ret;
};

struct genomic_interval_load_ret {
  std::vector<genomic_interval> gis;
  std::error_code ec;
};

template <>
struct std::formatter<genomic_interval> : std::formatter<std::string> {
  auto
  format(const genomic_interval &gi, std::format_context &ctx) const {
    return std::formatter<std::string>::format(
      std::format("{}\t{}\t{}", gi.ch_id, gi.start, gi.stop), ctx);
  }
};

#endif  // SRC_GENOMIC_INTERVAL_HPP_
