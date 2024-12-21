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

#include <algorithm>
#include <cstdint>
#include <format>
#include <ranges>  // IWYU pragma: keep
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>  // std::unreachable
#include <variant>
#include <vector>

struct cpg_index_metadata;

struct genomic_interval {
  static constexpr auto not_a_chrom{-1};
  std::int32_t ch_id{not_a_chrom};
  std::uint32_t start{};
  std::uint32_t stop{};

  auto
  operator<=>(const genomic_interval &) const = default;

  [[nodiscard]] static auto
  load(const cpg_index_metadata &index, const std::string &filename)
    -> std::tuple<std::vector<genomic_interval>, std::error_code>;
};

// ADS: Sorted intervals have chromosomes together but the order on
// chroms is arbitrary; then they are ordered by first coord position
// and second position is not relevant.
[[nodiscard]] auto
intervals_sorted(const cpg_index_metadata &cim,
                 const std::vector<genomic_interval> &gis) -> bool;

[[nodiscard]] inline auto
intervals_valid(const auto &g) -> bool {
  return std::ranges::all_of(g, [](const auto x) { return x.start <= x.stop; });
};

template <>
struct std::formatter<genomic_interval> : std::formatter<std::string> {
  auto
  format(const genomic_interval &gi, std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}\t{}\t{}", gi.ch_id, gi.start, gi.stop);
  }
};

// genomic_interval errors

enum class genomic_interval_code : std::uint32_t {
  ok = 0,
  error_parsing_bed_line = 1,
  chrom_name_not_found_in_index = 2,
  interval_past_chrom_end_in_index = 3,
};

// register genomic_interval_code as error code enum
template <>
struct std::is_error_code_enum<genomic_interval_code> : public std::true_type {
};

// category to provide text descriptions
struct genomic_interval_category : std::error_category {
  auto
  name() const noexcept -> const char * override {
    return "genomic_interval";
  }
  auto
  message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    // clang-format off
    switch (code) {
    case 0: return "ok"s;
    case 1: return "error parsing BED line"s;
    case 2: return "chrom name not found in index"s;
    case 3: return "interval past chrom end in index"s;
    }
    // clang-format on
    std::unreachable();  // hopefully
  }
};

inline auto
make_error_code(genomic_interval_code e) -> std::error_code {
  static auto category = genomic_interval_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_GENOMIC_INTERVAL_HPP_
