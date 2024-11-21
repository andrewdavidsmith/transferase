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

#include "cpg_index.hpp"
#include "genomic_interval.hpp"
#include "methylome.hpp"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <ranges>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <zlib.h>

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
write_intervals(std::ostream &out, const cpg_index &index,
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
    const std::string chrom{index.chrom_order[ch_id]};
    std::ranges::copy(chrom, buf.data());
    buf[size(chrom)] = delim;
    for (const auto &[gi, res] : chunk) {
      std::to_chars_result tcr{buf.data() + size(chrom) + 1, std::errc()};
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wstringop-overflow=0"
#endif
      tcr = std::to_chars(tcr.ptr, buf_end, gi.start);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, gi.stop);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, res.n_meth);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, res.n_unmeth);
      if constexpr (std::is_same<counts_res_type, counts_res_cov>::value) {
        *tcr.ptr++ = delim;
        tcr = std::to_chars(tcr.ptr, buf_end, res.n_covered);
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
write_bedgraph(std::ostream &out, const cpg_index &index,
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
    const std::string chrom{index.chrom_order[ch_id]};
    std::ranges::copy(chrom, buf.data());
    buf[size(chrom)] = delim;
    for (const auto &[gi, score] : chunk) {
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
      tcr = std::to_chars(tcr.ptr, buf_end, score, std::chars_format::general,
                          score_precision);
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
           const cpg_index &index, const auto &results) -> std::error_code {
  static constexpr auto buf_size{512};
  static constexpr auto delim{'\t'};

  using counts_res_type =
    typename std::remove_cvref_t<decltype(results)>::value_type;

  std::array<char, buf_size> buf{};
  const auto buf_beg = buf.data();
  const auto buf_end = buf.data() + buf_size;

  auto res = std::cbegin(results);

  const auto zipped = std::views::zip(index.chrom_size, index.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    std::ranges::copy(chrom_name, buf_beg);
    buf[size(chrom_name)] = delim;
    for (std::uint32_t bin_beg = 0; bin_beg < chrom_size; bin_beg += bin_size) {
      const auto bin_end = std::min(bin_beg + bin_size, chrom_size);
      std::to_chars_result tcr{buf_beg + std::size(chrom_name) + 1,
                               std::errc()};
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wstringop-overflow=0"
#endif
      tcr = std::to_chars(tcr.ptr, buf_end, bin_beg);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, bin_end);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, res->n_meth);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, res->n_unmeth);
      if constexpr (std::is_same<counts_res_type, counts_res_cov>::value) {
        *tcr.ptr++ = delim;
        tcr = std::to_chars(tcr.ptr, buf_end, res->n_covered);
      }
      *tcr.ptr++ = '\n';
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic pop
#endif
      out.write(buf_beg, std::ranges::distance(buf_beg, tcr.ptr));
      if (!out)
        return std::make_error_code(std::errc(errno));
      ++res;
    }
  }
  assert(res == std::cend(results));
  return {};
}

[[nodiscard]] inline auto
duration(const auto start, const auto stop) {
  const auto d = stop - start;
  return std::chrono::duration_cast<std::chrono::duration<double>>(d).count();
}

template <typename T, typename U>
inline auto
round_to_fit(U &a, U &b) -> void {
  // ADS: assign the max of a and b to be the max possible value; the
  // other one gets a fractional value then multiplied by max possible
  const U c = std::max(a, b);
  a = (a == c) ? std::numeric_limits<T>::max()
               : std::round((a / static_cast<double>(c)) *
                            std::numeric_limits<T>::max());
  b = (b == c) ? std::numeric_limits<T>::max()
               : std::round((b / static_cast<double>(c)) *
                            std::numeric_limits<T>::max());
}

template <typename T, typename U>
inline auto
conditional_round_to_fit(U &a, U &b) -> void {
  // ADS: optimization possible here?
  if (std::max(a, b) > std::numeric_limits<T>::max())
    round_to_fit<T>(a, b);
}

// ADS: this function should be replaced by one that can operate on a
// the data as though it were serealized but without reading the file
[[nodiscard]] inline auto
get_adler(const std::string &filename) -> std::uint64_t {
  const auto filesize = std::filesystem::file_size(filename);
  std::vector<char> buf(filesize);
  std::ifstream in(filename);
  in.read(buf.data(), filesize);
  return adler32_z(0, reinterpret_cast<std::uint8_t *>(buf.data()), filesize);
}

// ADS: this function should be replaced by one that can operate on a
// the data as though it were serealized but without reading the file
[[nodiscard]] inline auto
get_adler(const std::string &filename, std::error_code &ec) -> std::uint64_t {
  const auto filesize = std::filesystem::file_size(filename, ec);
  if (ec)
    return 0;
  std::vector<char> buf(filesize);
  std::ifstream in(filename);
  if (!in) {
    ec = std::make_error_code(std::errc(errno));
    return 0;
  }
  if (!in.read(buf.data(), filesize)) {
    ec = std::make_error_code(std::errc(errno));
    return 0;
  }
  return adler32_z(0, reinterpret_cast<std::uint8_t *>(buf.data()), filesize);
}

/*
  print std::filesystem::path
 */
template <>
struct std::formatter<std::filesystem::path> : std::formatter<std::string> {
  auto format(const std::filesystem::path &p, std::format_context &ctx) const {
    return std::formatter<std::string>::format(p.string(), ctx);
  }
};

auto
get_mxe_config_dir_default(std::error_code &ec) -> std::string;

/*
  auto
  check_mxe_config_dir(const std::string &dirname, std::error_code &ec) -> bool;
*/

#endif  // SRC_UTILITIES_HPP_
