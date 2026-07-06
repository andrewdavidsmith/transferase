/* MIT License
 *
 * Copyright (c) 2024-2025 Andrew D Smith
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

#include "genome_index.hpp"
#include "genome_index_data.hpp"
#include "genome_index_metadata.hpp"
#include "hash.hpp"
#include "level_container.hpp"
#include "level_element.hpp"
#include "methylome_metadata.hpp"
#include "query_container.hpp"
#include "query_element.hpp"  // implicitly used
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

#ifdef BENCHMARK
#include <chrono>
#include <iostream>  // for std::cerr
#include <print>
#endif

namespace transferase {

[[nodiscard]] auto
methylome_data::get_n_cpgs_from_file(
  const std::string &filename, std::error_code &ec) noexcept -> std::uint32_t {
  const auto filesize = std::filesystem::file_size(filename, ec);
  if (ec)
    return 0;
  return filesize / sizeof(mcount_pair);
}

[[nodiscard]] auto
methylome_data::get_n_cpgs_from_file(const std::string &filename) noexcept
  -> std::uint32_t {
  std::error_code ec;
  return get_n_cpgs_from_file(filename, ec);
}

[[nodiscard]] auto
methylome_data_read(const std::string &filename,
                    const methylome_metadata &metadata,
                    std::error_code &ec) noexcept -> methylome_data {
  static const auto io_error = std::make_error_code(std::errc::io_error);
  const auto filesize =
    static_cast<std::streamsize>(std::filesystem::file_size(filename, ec));
  if (ec)
    return {};
  std::ifstream in(filename);
  if (!in) {
    ec = std::make_error_code(std::errc(errno));
    return {};
  }

  // NOLINTBEGIN(*-reinterpret-cast)
  methylome_data meth;
  if (metadata.is_compressed) {
    std::vector<std::uint8_t> buf(filesize);
    const bool read_ok = static_cast<bool>(
      in.read(reinterpret_cast<char *>(std::data(buf)), filesize));
    const auto n_bytes = in.gcount();
    if (!read_ok || n_bytes != static_cast<std::streamsize>(filesize)) {
      ec = io_error;
      return {};
    }
    meth.cpgs.resize(metadata.n_cpgs);
#ifdef BENCHMARK
    const auto decompress_start{std::chrono::high_resolution_clock::now()};
#endif
    ec = decompress(buf, meth.cpgs);
#ifdef BENCHMARK
    const auto decompress_stop{std::chrono::high_resolution_clock::now()};
    const auto delta = decompress_stop - decompress_start;
    std::println(
      "decompress(buf, cpgs) time: {}us",
      std::chrono::duration_cast<std::chrono::microseconds>(delta).count());
#endif
    return meth;
  }
  meth.cpgs.resize(metadata.n_cpgs);
  const bool read_ok = static_cast<bool>(
    in.read(reinterpret_cast<char *>(std::data(meth.cpgs)), filesize));
  const auto n_bytes = in.gcount();
  if (!read_ok || n_bytes != static_cast<std::streamsize>(filesize)) {
    ec = io_error;
    return {};
  }
  // NOLINTEND(*-reinterpret-cast)

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
                     std::error_code &ec) noexcept -> methylome_data {
  return methylome_data_read(
    make_methylome_data_filename(dirname, methylome_name), meta, ec);
}

[[nodiscard]] auto
methylome_data::write(const std::string &filename,
                      const bool zip) const noexcept -> std::error_code {
  static const auto io_error = std::make_error_code(std::errc::io_error);
  std::vector<std::uint8_t> buf;
  if (zip) {
#ifdef BENCHMARK
    const auto compress_start{std::chrono::high_resolution_clock::now()};
#endif
    const auto compress_err = compress(cpgs, buf);
#ifdef BENCHMARK
    const auto compress_stop{std::chrono::high_resolution_clock::now()};
    const auto delta = compress_stop - compress_start;
    std::println(
      std::cerr, "compress(cpgs, buf) time: {}us",
      std::chrono::duration_cast<std::chrono::microseconds>(delta).count());
#endif
    if (compress_err)
      return compress_err;
  }

  std::ofstream out(filename, std::ios::binary);
  if (!out)
    return std::make_error_code(std::errc(errno));

  // NOLINTBEGIN(*-reinterpret-cast)
  if (zip) {
    const auto n_bytes = static_cast<std::streamsize>(std::size(buf));
    if (!out.write(reinterpret_cast<const char *>(std::data(buf)), n_bytes))
      return io_error;
  }
  else {
    const auto n_bytes =
      static_cast<std::streamsize>(std::size(cpgs) * record_size);
    if (!out.write(reinterpret_cast<const char *>(std::data(cpgs)), n_bytes))
      return io_error;
  }
  // NOLINTEND(*-reinterpret-cast)
  return std::error_code{};
}

auto
methylome_data::add(const methylome_data &rhs) noexcept -> void {
  // this follows the operator+= pattern
  assert(std::size(cpgs) == std::size(rhs.cpgs));
  std::ranges::transform(cpgs, rhs.cpgs, std::begin(cpgs),
                         [](const auto &l, const auto &r) -> mcount_pair {
                           return {
                             static_cast<mcount_t>(l.n_meth + r.n_meth),
                             static_cast<mcount_t>(l.n_unmeth + r.n_unmeth)};
                         });
}

template <typename U, typename T>
[[nodiscard]] static inline auto
get_levels_impl(const T b, const T e) noexcept -> U {
  U u;
  for (auto cursor = b; cursor != e; ++cursor) {
    u.n_meth += cursor->n_meth;
    u.n_unmeth += cursor->n_unmeth;
    if constexpr (std::is_same<U, level_element_covered_t>::value)
      u.n_covered += (*cursor != mcount_pair{});
  }
  return u;
}

template <>
[[nodiscard]] auto
methylome_data::get_levels<level_element_t>(
  const transferase::query_container &query) const noexcept
  -> level_container<level_element_t> {
  std::vector<level_element_t> res(std::size(query));
  const auto beg = std::cbegin(cpgs);
  // for (const auto [i, q] : std::views::enumerate(query))
  std::uint32_t i = 0;
  for (const auto q : query)
    res[i++] = get_levels_impl<level_element_t>(beg + q.start, beg + q.stop);
  return level_container<level_element_t>(std::move(res));
}

template <>
auto
methylome_data::get_levels<level_element_t>(
  const transferase::query_container &query,
  level_container<level_element_t>::iterator &res) const noexcept -> void {
  const auto beg = std::cbegin(cpgs);
  // for (const auto [i, q] : std::views::enumerate(query))
  for (const auto q : query)
    *res++ = get_levels_impl<level_element_t>(beg + q.start, beg + q.stop);
}

template <>
[[nodiscard]] auto
methylome_data::get_levels<level_element_covered_t>(
  const transferase::query_container &query) const noexcept
  -> level_container<level_element_covered_t> {
  std::vector<level_element_covered_t> res(std::size(query));
  const auto beg = std::cbegin(cpgs);
  // for (const auto [i, q] : std::views::enumerate(query))
  std::uint32_t i = 0;
  for (const auto q : query)
    res[i++] =
      get_levels_impl<level_element_covered_t>(beg + q.start, beg + q.stop);
  return level_container<level_element_covered_t>(std::move(res));
}

template <>
auto
methylome_data::get_levels<level_element_covered_t>(
  const transferase::query_container &query,
  typename level_container<level_element_covered_t>::iterator &res)
  const noexcept -> void {
  const auto beg = std::cbegin(cpgs);
  // for (const auto [i, q] : std::views::enumerate(query))
  for (const auto q : query)
    *res++ =
      get_levels_impl<level_element_covered_t>(beg + q.start, beg + q.stop);
}

// bins code

template <typename T>
static auto
bin_levels_impl(genome_index_data::vec::const_iterator &posn_itr,
                const genome_index_data::vec::const_iterator posn_end,
                const std::uint32_t bin_end,
                methylome_data::vec::const_iterator &cpg_itr) noexcept -> T {
  T t{};
  while (posn_itr != posn_end && *posn_itr < bin_end) {
    t.n_meth += cpg_itr->n_meth;
    t.n_unmeth += cpg_itr->n_unmeth;
    if constexpr (std::is_same<T, level_element_covered_t>::value)
      t.n_covered += (*cpg_itr != mcount_pair{});
    ++cpg_itr;
    ++posn_itr;
  }
  return t;
}

template <typename T>
[[nodiscard]] static auto
get_levels_impl(const std::uint32_t bin_size, const genome_index &index,
                const methylome_data::vec &cpgs) noexcept
  -> level_container<T> {
  std::vector<T> results;
  // reserving all the bins; note the container returned from this function is
  // a 1D container.
  results.reserve(index.get_n_bins(bin_size));
  const auto zipped = std::views::zip(
    index.data.positions, index.meta.chrom_size, index.meta.chrom_offset);
  for (const auto [positions, chrom_size, offset] : zipped) {
    auto posn_itr = std::cbegin(positions);
    const auto posn_end = std::cend(positions);
    auto cpg_itr = std::cbegin(cpgs) + offset;
    for (std::uint32_t i = 0; i < chrom_size; i += bin_size) {
      // need bin_end like this because otherwise we go into next chrom
      const auto bin_end = std::min(i + bin_size, chrom_size);
      results.push_back(
        bin_levels_impl<T>(posn_itr, posn_end, bin_end, cpg_itr));
    }
  }
  return level_container<T>(std::move(results));
}

template <typename T>
static auto
get_levels_impl(
  const std::uint32_t bin_size, const genome_index &index,
  const methylome_data::vec &cpgs,
  typename transferase::level_container<T>::iterator &res) noexcept -> void {
  const auto zipped = std::views::zip(
    index.data.positions, index.meta.chrom_size, index.meta.chrom_offset);
  for (const auto [positions, chrom_size, offset] : zipped) {
    auto posn_itr = std::cbegin(positions);
    const auto posn_end = std::cend(positions);
    auto cpg_itr = std::cbegin(cpgs) + offset;
    for (std::uint32_t i = 0; i < chrom_size; i += bin_size) {
      // need bin_end like this because otherwise we go into next chrom
      const auto bin_end = std::min(i + bin_size, chrom_size);
      *res++ = bin_levels_impl<T>(posn_itr, posn_end, bin_end, cpg_itr);
    }
  }
}

template <>
[[nodiscard]] auto
methylome_data::get_levels<level_element_t>(const std::uint32_t bin_size,
                                            const genome_index &index)
  const noexcept -> level_container<level_element_t> {
  return get_levels_impl<level_element_t>(bin_size, index, cpgs);
}

template <>
auto
methylome_data::get_levels<level_element_t>(
  const std::uint32_t bin_size, const genome_index &index,
  level_container<level_element_t>::iterator &res) const noexcept -> void {
  get_levels_impl<level_element_t>(bin_size, index, cpgs, res);
}

template <>
[[nodiscard]] auto
methylome_data::get_levels<level_element_covered_t>(
  const std::uint32_t bin_size, const genome_index &index) const noexcept
  -> level_container<level_element_covered_t> {
  return get_levels_impl<level_element_covered_t>(bin_size, index, cpgs);
}

template <>
auto
methylome_data::get_levels<level_element_covered_t>(
  const std::uint32_t bin_size, const genome_index &index,
  level_container<level_element_covered_t>::iterator &res) const noexcept
  -> void {
  get_levels_impl<level_element_covered_t>(bin_size, index, cpgs, res);
}

// sliding window code

template <typename lvl_elem_t>
static auto
include_window_levels_impl(
  genome_index_data::vec::const_iterator &posn_itr,
  const genome_index_data::vec::const_iterator posn_end,
  const std::uint32_t window_end, methylome_data::vec::const_iterator &cpg_itr,
  lvl_elem_t &t) noexcept {
  while (posn_itr != posn_end && *posn_itr < window_end) {
    t.n_meth += cpg_itr->n_meth;
    t.n_unmeth += cpg_itr->n_unmeth;
    if constexpr (std::is_same<lvl_elem_t, level_element_covered_t>::value)
      t.n_covered += (*cpg_itr != mcount_pair{});
    ++cpg_itr;
    ++posn_itr;
  }
}

template <typename lvl_elem_t>
static auto
exclude_window_levels_impl(
  genome_index_data::vec::const_iterator &posn_itr,
  const genome_index_data::vec::const_iterator posn_end,
  const std::uint32_t window_end, methylome_data::vec::const_iterator &cpg_itr,
  lvl_elem_t &t) noexcept {
  while (posn_itr != posn_end && *posn_itr < window_end) {
    t.n_meth -= cpg_itr->n_meth;
    t.n_unmeth -= cpg_itr->n_unmeth;
    if constexpr (std::is_same<lvl_elem_t, level_element_covered_t>::value)
      t.n_covered -= (*cpg_itr != mcount_pair{});
    ++cpg_itr;
    ++posn_itr;
  }
}

template <typename T>
[[nodiscard]] static auto
get_levels_impl(const std::uint32_t window_size,
                const std::uint32_t window_step, const genome_index &index,
                const methylome_data::vec &cpgs) noexcept
  -> level_container<T> {
  std::vector<T> results;
  // reserving all the windows; note the container returned from this function
  // is a 1D container.
  results.reserve(index.get_n_windows(window_size));
  const auto zipped = std::views::zip(
    index.data.positions, index.meta.chrom_size, index.meta.chrom_offset);
  for (const auto [positions, chrom_size, offset] : zipped) {
    auto leading_posn_itr = std::cbegin(positions);
    auto lagging_posn_itr = leading_posn_itr;
    const auto posn_end = std::cend(positions);
    auto leading_cpg_itr = std::cbegin(cpgs) + offset;
    auto lagging_cpg_itr = leading_cpg_itr;
    T t{};
    for (std::uint32_t window_beg = 0; window_beg < chrom_size;
         window_beg += window_step) {
      exclude_window_levels_impl(lagging_posn_itr, posn_end, window_beg,
                                 lagging_cpg_itr, t);
      const auto window_end = std::min(window_beg + window_size, chrom_size);
      include_window_levels_impl(leading_posn_itr, posn_end, window_end,
                                 leading_cpg_itr, t);
      results.push_back(t);
    }
  }
  return level_container<T>(std::move(results));
}

template <typename lvl_elem_t>
static auto
get_levels_impl(
  // clang-format off
  const std::uint32_t window_size,
  const std::uint32_t window_step,
  const genome_index &index,
  const methylome_data::vec &cpgs,
  typename transferase::level_container<lvl_elem_t>::iterator &res_itr
  // clang-format on
  ) noexcept -> void {
  const auto zipped = std::views::zip(
    index.data.positions, index.meta.chrom_size, index.meta.chrom_offset);
  for (const auto [positions, chrom_size, offset] : zipped) {
    auto leading_posn_itr = std::cbegin(positions);
    auto lagging_posn_itr = leading_posn_itr;
    const auto posn_end = std::cend(positions);
    auto leading_cpg_itr = std::cbegin(cpgs) + offset;
    auto lagging_cpg_itr = leading_cpg_itr;
    lvl_elem_t t{};
    for (std::uint32_t window_beg = 0; window_beg < chrom_size;
         window_beg += window_step) {
      exclude_window_levels_impl(lagging_posn_itr, posn_end, window_beg,
                                 lagging_cpg_itr, t);
      const auto window_end = std::min(window_beg + window_size, chrom_size);
      include_window_levels_impl(leading_posn_itr, posn_end, window_end,
                                 leading_cpg_itr, t);
      *res_itr++ = t;
    }
  }
}

template <>
[[nodiscard]] auto
methylome_data::get_levels<level_element_t>(const std::uint32_t window_size,
                                            const std::uint32_t window_step,
                                            const genome_index &index)
  const noexcept -> level_container<level_element_t> {
  return get_levels_impl<level_element_t>(window_size, window_step, index,
                                          cpgs);
}

template <>
auto
methylome_data::get_levels<level_element_t>(
  const std::uint32_t window_size, const std::uint32_t window_step,
  const genome_index &index,
  level_container<level_element_t>::iterator &res) const noexcept -> void {
  get_levels_impl<level_element_t>(window_size, window_step, index, cpgs, res);
}

template <>
[[nodiscard]] auto
methylome_data::get_levels<level_element_covered_t>(
  const std::uint32_t window_size, const std::uint32_t window_step,
  const genome_index &index) const noexcept
  -> level_container<level_element_covered_t> {
  return get_levels_impl<level_element_covered_t>(window_size, window_step,
                                                  index, cpgs);
}

template <>
auto
methylome_data::get_levels<level_element_covered_t>(
  const std::uint32_t window_size, const std::uint32_t window_step,
  const genome_index &index,
  level_container<level_element_covered_t>::iterator &res) const noexcept
  -> void {
  get_levels_impl<level_element_covered_t>(window_size, window_step, index,
                                           cpgs, res);
}

// global code

template <>
[[nodiscard]] auto
methylome_data::global_levels<level_element_covered_t>() const noexcept
  -> level_element_covered_t {
  std::uint32_t n_meth{};
  std::uint32_t n_unmeth{};
  std::uint32_t n_covered{};
  for (const auto &cpg : cpgs) {
    n_meth += cpg.n_meth;
    n_unmeth += cpg.n_unmeth;
    n_covered += (cpg != mcount_pair{});
  }
  return {n_meth, n_unmeth, n_covered};
}

template <>
[[nodiscard]] auto
methylome_data::global_levels<level_element_t>() const noexcept
  -> level_element_t {
  std::uint32_t n_meth{};
  std::uint32_t n_unmeth{};
  for (const auto &cpg : cpgs) {
    n_meth += cpg.n_meth;
    n_unmeth += cpg.n_unmeth;
  }
  return {n_meth, n_unmeth};
}

[[nodiscard]] auto
methylome_data::hash() const noexcept -> std::uint64_t {
  return get_adler(std::data(cpgs), std::size(cpgs) * record_size);
}

[[nodiscard]] auto
methylome_data::get_n_cpgs() const noexcept -> std::uint32_t {
  return std::size(cpgs);
}

}  // namespace transferase
