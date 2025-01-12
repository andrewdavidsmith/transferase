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

#include "genome_index.hpp"
#include "genome_index_data.hpp"
#include "genome_index_metadata.hpp"
#include "hash.hpp"
#include "methylome_metadata.hpp"
#include "query_container.hpp"
#include "query_element.hpp"  // for xfrase::q_elem_t
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

namespace transferase {

[[nodiscard]] auto
methylome_data::get_n_cpgs_from_file(
  const std::string &filename, std::error_code &ec) noexcept -> std::uint32_t {
  const auto filesize = std::filesystem::file_size(filename, ec);
  if (ec)
    return 0;
  return filesize / sizeof(m_count_p);
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
  const auto filesize =
    static_cast<std::streamsize>(std::filesystem::file_size(filename, ec));
  if (ec)
    return {};
  std::ifstream in(filename);
  if (!in) {
    ec = std::make_error_code(std::errc(errno));
    return {};
  }

  // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
  methylome_data meth;
  if (metadata.is_compressed) {
    std::vector<std::uint8_t> buf(filesize);
    const bool read_ok = static_cast<bool>(
      in.read(reinterpret_cast<char *>(buf.data()), filesize));
    const auto n_bytes = in.gcount();
    if (!read_ok || n_bytes != static_cast<std::streamsize>(filesize)) {
      ec = methylome_data_error_code::error_reading;
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
    ec = methylome_data_error_code::error_reading;
    return {};
  }

  // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

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

  // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
  if (zip) {
    const auto n_bytes = static_cast<std::streamsize>(std::size(buf));
    if (!out.write(reinterpret_cast<const char *>(buf.data()), n_bytes))
      return methylome_data_error_code::error_writing;
  }
  else {
    const auto n_bytes =
      static_cast<std::streamsize>(std::size(cpgs) * record_size);
    if (!out.write(reinterpret_cast<const char *>(cpgs.data()), n_bytes))
      return methylome_data_error_code::error_writing;
  }
  // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
  return std::error_code{};
}

auto
methylome_data::add(const methylome_data &rhs) noexcept -> void {
  // this follows the operator+= pattern
  assert(std::size(cpgs) == std::size(rhs.cpgs));
  std::ranges::transform(cpgs, rhs.cpgs, std::begin(cpgs),
                         [](const auto &l, const auto &r) -> m_count_p {
                           return {
                             static_cast<m_count_t>(l.n_meth + r.n_meth),
                             static_cast<m_count_t>(l.n_unmeth + r.n_unmeth)};
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
      u.n_covered += (*cursor != m_count_p{});
  }
  return u;
}

template <typename U>
[[nodiscard]] static inline auto
get_levels_impl(const methylome_data::vec &cpgs,
                const genome_index_data::vec &positions,
                const std::uint32_t offset, const transferase::q_elem_t start,
                const transferase::q_elem_t stop) noexcept -> U {
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
  return get_levels_impl<U>(cpg_beg, cpg_end);
}

[[nodiscard]] auto
methylome_data::get_levels_covered(const transferase::query_container &query)
  const noexcept -> level_container<level_element_covered_t> {
  auto res = level_container<level_element_covered_t>(size(query));
  const auto beg = std::cbegin(cpgs);
  for (const auto [i, q] : std::views::enumerate(query))
    res[i] =
      get_levels_impl<level_element_covered_t>(beg + q.start, beg + q.stop);
  return res;
}

[[nodiscard]] auto
methylome_data::get_levels(const transferase::query_container &query)
  const noexcept -> level_container<level_element_t> {
  std::vector<level_element_t> res(size(query));
  const auto beg = std::cbegin(cpgs);
  for (const auto [i, q] : std::views::enumerate(query))
    res[i] = get_levels_impl<level_element_t>(beg + q.start, beg + q.stop);
  return level_container<level_element_t>(std::move(res));
}

[[nodiscard]] auto
methylome_data::global_levels_covered() const noexcept
  -> level_element_covered_t {
  std::uint32_t n_meth{};
  std::uint32_t n_unmeth{};
  std::uint32_t n_covered{};
  for (const auto &cpg : cpgs) {
    n_meth += cpg.n_meth;
    n_unmeth += cpg.n_unmeth;
    n_covered += (cpg != m_count_p{});
  }
  return {n_meth, n_unmeth, n_covered};
}

[[nodiscard]] auto
methylome_data::global_levels() const noexcept -> level_element_t {
  std::uint32_t n_meth{};
  std::uint32_t n_unmeth{};
  for (const auto &cpg : cpgs) {
    n_meth += cpg.n_meth;
    n_unmeth += cpg.n_unmeth;
  }
  return {n_meth, n_unmeth};
}

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
      t.n_covered += (*cpg_itr == m_count_p{});
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
        bin_levels_impl<T>(posn_itr, posn_end, bin_end, cpg_itr));
    }
  }
  return level_container<T>(std::move(results));
}

[[nodiscard]] auto
methylome_data::get_levels(const std::uint32_t bin_size,
                           const genome_index &index) const noexcept
  -> level_container<level_element_t> {
  return get_levels_impl<level_element_t>(bin_size, index, cpgs);
}

[[nodiscard]] auto
methylome_data::get_levels_covered(const std::uint32_t bin_size,
                                   const genome_index &index) const noexcept
  -> level_container<level_element_covered_t> {
  return get_levels_impl<level_element_covered_t>(bin_size, index, cpgs);
}

[[nodiscard]] auto
methylome_data::hash() const noexcept -> std::uint64_t {
  return get_adler(cpgs.data(), std::size(cpgs) * record_size);
}

[[nodiscard]] auto
methylome_data::get_n_cpgs() const noexcept -> std::uint32_t {
  return std::size(cpgs);
}

}  // namespace transferase
