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

#ifndef SRC_UTILITIES_HPP_
#define SRC_UTILITIES_HPP_

/*
  Functions declared here are used by multiple source files
 */

#include "cpg_index_meta.hpp"
#include "genomic_interval.hpp"
#include "methylome_results_types.hpp"  // counts_res and counts_res_cov

#include <zlib.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <limits>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

struct mxe_to_chars_result {
  char *ptr{};
  std::error_code error{};
};
typedef mxe_to_chars_result compose_result;

struct mxe_from_chars_result {
  const char *ptr{};
  std::error_code error{};
};
typedef mxe_from_chars_result parse_result;

auto
write_intervals(std::ostream &out, const cpg_index_meta &cim,
                const std::vector<genomic_interval> &gis,
                std::ranges::input_range auto &&results) -> std::error_code {
  static constexpr auto buf_size{512};
  static constexpr auto delim{'\t'};

  std::array<char, buf_size> buf{};
  const auto buf_end = buf.data() + buf_size;

  using counts_res_type =
    typename std::remove_cvref_t<decltype(results)>::value_type;
  using gis_res = std::pair<const genomic_interval &, counts_res_type>;
  const auto same_chrom = [](const gis_res &a, const gis_res &b) {
    return a.first.ch_id == b.first.ch_id;
  };

  for (const auto &chunk :
       std::views::zip(gis, results) | std::views::chunk_by(same_chrom)) {
    const auto ch_id = get<0>(chunk.front()).ch_id;
    const std::string chrom{cim.chrom_order[ch_id]};
    std::ranges::copy(chrom, buf.data());
    buf[size(chrom)] = delim;
    for (const auto &[gi, single_result] : chunk) {
      std::to_chars_result tcr{buf.data() + size(chrom) + 1, std::errc()};
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wstringop-overflow=0"
#endif
      tcr = std::to_chars(tcr.ptr, buf_end, gi.start);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, gi.stop);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, single_result.n_meth);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, single_result.n_unmeth);
      if constexpr (std::is_same<counts_res_type, counts_res_cov>::value) {
        *tcr.ptr++ = delim;
        tcr = std::to_chars(tcr.ptr, buf_end, single_result.n_covered);
      }
      *tcr.ptr++ = '\n';
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic pop
#endif
      out.write(buf.data(), std::ranges::distance(buf.data(), tcr.ptr));
      if (!out)
        return std::make_error_code(std::errc(errno));
    }
  }
  return {};
}

auto
write_bedgraph(std::ostream &out, const cpg_index_meta &cim,
               const std::vector<genomic_interval> &gis,
               std::ranges::input_range auto &&scores) -> std::error_code {
  static constexpr auto score_precision{6};
  static constexpr auto buf_size{512};
  static constexpr auto delim{'\t'};

  std::array<char, buf_size> buf{};
  const auto buf_end = buf.data() + buf_size;

  using gis_score = std::pair<const genomic_interval &, const double>;
  const auto same_chrom = [](const gis_score &a, const gis_score &b) {
    return a.first.ch_id == b.first.ch_id;
  };

  for (const auto &chunk :
       std::views::zip(gis, scores) | std::views::chunk_by(same_chrom)) {
    const auto ch_id = get<0>(chunk.front()).ch_id;
    const std::string chrom{cim.chrom_order[ch_id]};
    std::ranges::copy(chrom, buf.data());
    buf[size(chrom)] = delim;
    for (const auto &[gi, single_score] : chunk) {
      std::to_chars_result tcr{buf.data() + size(chrom) + 1, std::errc()};
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wstringop-overflow=0"
#endif
      tcr = std::to_chars(tcr.ptr, buf_end, gi.start);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, gi.stop);
      *tcr.ptr++ = delim;
      // ADS: below the format is intended to mimick default for cout
      tcr = std::to_chars(tcr.ptr, buf_end, single_score,
                          std::chars_format::general, score_precision);
      *tcr.ptr++ = '\n';
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic pop
#endif
      out.write(buf.data(), std::ranges::distance(buf.data(), tcr.ptr));
      if (!out)
        return std::make_error_code(std::errc(errno));
    }
  }
  return {};
}

auto
write_bins(std::ostream &out, const std::uint32_t bin_size,
           const cpg_index_meta &cim, const auto &results) -> std::error_code {
  static constexpr auto buf_size{512};
  static constexpr auto delim{'\t'};

  using counts_res_type =
    typename std::remove_cvref_t<decltype(results)>::value_type;

  std::array<char, buf_size> buf{};
  const auto buf_end = buf.data() + buf_size;

  auto results_itr = std::cbegin(results);

  const auto zipped = std::views::zip(cim.chrom_size, cim.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    std::ranges::copy(chrom_name, buf.data());
    buf[size(chrom_name)] = delim;
    for (std::uint32_t bin_beg = 0; bin_beg < chrom_size; bin_beg += bin_size) {
      const auto bin_end = std::min(bin_beg + bin_size, chrom_size);
      std::to_chars_result tcr{buf.data() + std::size(chrom_name) + 1,
                               std::errc()};
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wstringop-overflow=0"
#endif
      tcr = std::to_chars(tcr.ptr, buf_end, bin_beg);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, bin_end);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, results_itr->n_meth);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, results_itr->n_unmeth);
      if constexpr (std::is_same<counts_res_type, counts_res_cov>::value) {
        *tcr.ptr++ = delim;
        tcr = std::to_chars(tcr.ptr, buf_end, results_itr->n_covered);
      }
      *tcr.ptr++ = '\n';
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic pop
#endif
      out.write(buf.data(), std::ranges::distance(buf.data(), tcr.ptr));
      if (!out)
        return std::make_error_code(std::errc(errno));
      ++results_itr;
    }
  }
  assert(results_itr == std::cend(results));
  return {};
}

[[nodiscard]] inline auto
duration(const auto start, const auto stop) {
  const auto d = stop - start;
  return std::chrono::duration_cast<std::chrono::duration<double>>(d).count();
}

template <>
struct std::formatter<std::filesystem::path> : std::formatter<std::string> {
  auto
  format(const std::filesystem::path &p, std::format_context &ctx) const {
    return std::formatter<std::string>::format(p.string(), ctx);
  }
};

[[nodiscard]] auto
get_mxe_config_dir_default(std::error_code &ec) -> std::string;

/*
  auto
  check_mxe_config_dir(const std::string &dirname, std::error_code &ec) -> bool;
*/

[[nodiscard]] inline auto
strip(char const *const x) -> const std::string_view {
  const std::string s{x};
  const auto start = s.find_first_not_of("\n\r");
  return std::string_view(x + start, x + std::size(s));
}

[[nodiscard]] auto
get_username() -> std::tuple<std::string, std::error_code>;

[[nodiscard]] auto
get_time_as_string() -> std::string;

#endif  // SRC_UTILITIES_HPP_
