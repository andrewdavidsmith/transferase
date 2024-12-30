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

#include "methylome_data.hpp"

#include "cpg_index.hpp"
#include "cpg_index_metadata.hpp"
#include "hash.hpp"
#include "methylome_metadata.hpp"
#include "methylome_results_types.hpp"
#include "query.hpp"
#include "zlib_adapter.hpp"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdint>  // for uint32_t, uint16_t, uint8_t, uint64_t
#include <filesystem>
#include <format>
#include <fstream>
#include <ranges>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>  // for is_same
#include <utility>      // for pair, move
#include <vector>

#ifdef XFRASE_BENCHMARK
#include <chrono>
#include <iostream>  // for std::cerr
#include <print>
#endif

namespace xfrase {

[[nodiscard]] auto
methylome_data::get_n_cpgs_from_file(const std::string &filename,
                                     std::error_code &ec) -> std::uint32_t {
  const auto filesize = std::filesystem::file_size(filename, ec);
  if (ec)
    return 0;
  return filesize / sizeof(m_count_elem);
}

[[nodiscard]] auto
methylome_data::get_n_cpgs_from_file(const std::string &filename)
  -> std::uint32_t {
  std::error_code ec;
  return get_n_cpgs_from_file(filename, ec);
}

[[nodiscard]] auto
methylome_data::read(const std::string &filename,
                     const methylome_metadata &metadata,
                     std::error_code &ec) -> methylome_data {
  const auto filesize = std::filesystem::file_size(filename, ec);
  if (ec)
    return {};
  std::ifstream in(filename);
  if (!in) {
    ec = std::make_error_code(std::errc(errno));
    return {};
  }

  methylome_data meth;
  if (metadata.is_compressed) {
    std::vector<std::uint8_t> buf(filesize);
    const bool read_ok = static_cast<bool>(
      in.read(reinterpret_cast<char *>(buf.data()), filesize));
    const auto n_bytes = in.gcount();
    if (!read_ok || n_bytes != static_cast<std::streamsize>(filesize)) {
      ec = methylome_data_code::error_reading;
      return {};
    }
    meth.cpgs.resize(metadata.n_cpgs);
#ifdef BENCHMARK
    const auto decompress_start{std::chrono::high_resolution_clock::now()};
#endif
    ec = decompress(buf, meth.cpgs);
#ifdef BENCHMARK
    const auto decompress_stop{std::chrono::high_resolution_clock::now()};
    std::println("decompress(buf, cpgs) time: {}s",
                 duration(decompress_start, decompress_stop));
#endif
    return meth;
  }
  meth.cpgs.resize(metadata.n_cpgs);
  const bool read_ok = static_cast<bool>(
    in.read(reinterpret_cast<char *>(meth.cpgs.data()), filesize));
  const auto n_bytes = in.gcount();
  if (!read_ok || n_bytes != static_cast<std::streamsize>(filesize)) {
    ec = methylome_data_code::error_reading;
    return {};
  }

  ec = std::error_code{};
  return meth;
}

[[nodiscard]]
static inline auto
make_methylome_data_filename(const std::string &dirname,
                             const std::string &methylome_name) {
  const auto with_extension =
    std::format("{}{}", methylome_name, methylome_data::filename_extension);
  return (std::filesystem::path{dirname} / with_extension).string();
}

[[nodiscard]] auto
methylome_data::read(const std::string &dirname,
                     const std::string &methylome_name,
                     const methylome_metadata &meta,
                     std::error_code &ec) -> methylome_data {
  return read(make_methylome_data_filename(dirname, methylome_name), meta, ec);
}

[[nodiscard]] auto
methylome_data::write(const std::string &filename,
                      const bool zip) const -> std::error_code {
  std::vector<std::uint8_t> buf;
  if (zip) {
#ifdef BENCHMARK
    const auto compress_start{std::chrono::high_resolution_clock::now()};
#endif
    const auto compress_err = compress(cpgs, buf);
#ifdef BENCHMARK
    const auto compress_stop{std::chrono::high_resolution_clock::now()};
    std::println(std::cerr, "compress(cpgs, buf) time: {}s",
                 duration(compress_start, compress_stop));
#endif
    if (compress_err)
      return compress_err;
  }

  std::ofstream out(filename, std::ios::binary);
  if (!out)
    return std::make_error_code(std::errc(errno));

  if (zip) {
    if (!out.write(reinterpret_cast<const char *>(buf.data()), std::size(buf)))
      return methylome_data_code::error_writing;
  }
  else {
    if (!out.write(reinterpret_cast<const char *>(cpgs.data()),
                   std::size(cpgs) * record_size))
      return methylome_data_code::error_writing;
  }
  return std::error_code{};
}

auto
methylome_data::add(const methylome_data &rhs) -> void {
  // this follows the operator+= pattern
  assert(std::size(cpgs) == std::size(rhs.cpgs));
  std::ranges::transform(cpgs, rhs.cpgs, std::begin(cpgs),
                         [](const auto &l, const auto &r) -> m_elem {
                           return {l.first + r.first, l.second + r.second};
                         });
}

template <typename U, typename T>
[[nodiscard]] static inline auto
get_counts_impl(const T b, const T e) -> U {
  U u;
  for (auto cursor = b; cursor != e; ++cursor) {
    u.n_meth += cursor->first;
    u.n_unmeth += cursor->second;
    if constexpr (std::is_same<U, counts_res_cov>::value)
      u.n_covered += *cursor != std::pair<std::uint16_t, std::uint16_t>{};
  }
  return u;
}

template <typename U>
[[nodiscard]] static inline auto
get_counts_impl(const methylome_data::vec &cpgs,
                const cpg_index_data::vec &positions,
                const std::uint32_t offset, const xfrase::q_elem_t start,
                const xfrase::q_elem_t stop) -> U {
  // ADS: it is possible that the intervals requested are past the cpg
  // sites since they might be in the genome, but past the final cpg
  // site location. This code *should* be able to handle such a
  // situation.
  namespace rg = std::ranges;
  const auto cpg_beg_lb = rg::lower_bound(positions, start);
  const auto cpg_beg_dist = rg::distance(std::cbegin(positions), cpg_beg_lb);
  const auto cpg_beg = std::cbegin(cpgs) + offset + cpg_beg_dist;
  const auto cpg_end_lb =
    rg::lower_bound(cpg_beg_lb, std::cend(positions), stop);
  const auto cpg_end_dist = rg::distance(std::cbegin(positions), cpg_end_lb);
  const auto cpg_end = std::cbegin(cpgs) + offset + cpg_end_dist;
  return get_counts_impl<U>(cpg_beg, cpg_end);
}

[[nodiscard]] auto
methylome_data::get_counts_cov(
  const cpg_index_data::vec &positions, const std::uint32_t offset,
  const xfrase::q_elem_t start,
  const xfrase::q_elem_t stop) const -> counts_res_cov {
  return get_counts_impl<counts_res_cov>(cpgs, positions, offset, start, stop);
}

[[nodiscard]] auto
methylome_data::get_counts(const cpg_index_data::vec &positions,
                           const std::uint32_t offset,
                           const xfrase::q_elem_t start,
                           const xfrase::q_elem_t stop) const -> counts_res {
  return get_counts_impl<counts_res>(cpgs, positions, offset, start, stop);
}

[[nodiscard]] auto
methylome_data::get_counts_cov(const xfrase::q_elem_t start,
                               const xfrase::q_elem_t stop) const
  -> counts_res_cov {
  return get_counts_impl<counts_res_cov>(std::cbegin(cpgs) + start,
                                         std::cbegin(cpgs) + stop);
}

[[nodiscard]] auto
methylome_data::get_counts(const xfrase::q_elem_t start,
                           const xfrase::q_elem_t stop) const -> counts_res {
  return get_counts_impl<counts_res>(std::cbegin(cpgs) + start,
                                     std::cbegin(cpgs) + stop);
}

[[nodiscard]] auto
methylome_data::get_counts_cov(const xfrase::query &qry) const
  -> std::vector<counts_res_cov> {
  std::vector<counts_res_cov> res(size(qry));
  const auto beg = std::cbegin(cpgs);
  for (const auto [i, q] : std::views::enumerate(qry))
    res[i] = get_counts_impl<counts_res_cov>(beg + q.first, beg + q.second);
  return res;
}

[[nodiscard]] auto
methylome_data::get_counts(const xfrase::query &qry) const
  -> std::vector<counts_res> {
  std::vector<counts_res> res(size(qry));
  const auto beg = std::cbegin(cpgs);
  for (const auto [i, q] : std::views::enumerate(qry))
    res[i] = get_counts_impl<counts_res>(beg + q.first, beg + q.second);
  return res;
}

[[nodiscard]] auto
methylome_data::total_counts_cov() const -> counts_res_cov {
  m_count_acc_t n_meth{};
  m_count_acc_t n_unmeth{};
  std::uint32_t n_covered{};
  for (const auto &cpg : cpgs) {
    n_meth += cpg.first;
    n_unmeth += cpg.second;
    n_covered += cpg != std::pair<std::uint16_t, std::uint16_t>{};
  }
  return {n_meth, n_unmeth, n_covered};
}

[[nodiscard]] auto
methylome_data::total_counts() const -> counts_res {
  m_count_acc_t n_meth{};
  m_count_acc_t n_unmeth{};
  for (const auto &cpg : cpgs) {
    n_meth += cpg.first;
    n_unmeth += cpg.second;
  }
  return {n_meth, n_unmeth};
}

template <typename T>
static auto
bin_counts_impl(cpg_index_data::vec::const_iterator &posn_itr,
                const cpg_index_data::vec::const_iterator posn_end,
                const std::uint32_t bin_end,
                methylome_data::vec::const_iterator &cpg_itr) -> T {
  T t{};
  while (posn_itr != posn_end && *posn_itr < bin_end) {
    t.n_meth += cpg_itr->first;
    t.n_unmeth += cpg_itr->second;
    if constexpr (std::is_same<T, counts_res_cov>::value)
      t.n_covered += (*cpg_itr == m_count_elem{});
    ++cpg_itr;
    ++posn_itr;
  }
  return t;
}

template <typename T>
[[nodiscard]] static auto
get_bins_impl(const std::uint32_t bin_size, const cpg_index &index,
              const methylome_data::vec &cpgs) -> std::vector<T> {
  std::vector<T> results;  // ADS TODO: reserve n_bins

  const auto zipped = std::views::zip(
    index.data.positions, index.meta.chrom_size, index.meta.chrom_offset);
  for (const auto [positions, chrom_size, offset] : zipped) {
    auto posn_itr = std::cbegin(positions);
    const auto posn_end = std::cend(positions);
    auto cpg_itr = std::cbegin(cpgs) + offset;
    for (std::uint32_t i = 0; i < chrom_size; i += bin_size) {
      const auto bin_end = std::min(i + bin_size, chrom_size);
      results.emplace_back(
        bin_counts_impl<T>(posn_itr, posn_end, bin_end, cpg_itr));
    }
  }
  return results;
}

[[nodiscard]] auto
methylome_data::get_bins(const std::uint32_t bin_size, const cpg_index &index)
  const -> std::vector<counts_res> {
  return get_bins_impl<counts_res>(bin_size, index, cpgs);
}

[[nodiscard]] auto
methylome_data::get_bins_cov(const std::uint32_t bin_size,
                             const cpg_index &index) const
  -> std::vector<counts_res_cov> {
  return get_bins_impl<counts_res_cov>(bin_size, index, cpgs);
}

[[nodiscard]] auto
methylome_data::hash() const -> std::uint64_t {
  return get_adler(cpgs.data(), std::size(cpgs) * record_size);
}

[[nodiscard]] auto
methylome_data::get_n_cpgs() const -> std::uint32_t {
  return std::size(cpgs);
}

}  // namespace xfrase
