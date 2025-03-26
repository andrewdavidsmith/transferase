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

#include "genome_index_data.hpp"

#include "chrom_range.hpp"
#include "genome_index_data_impl.hpp"
#include "genome_index_metadata.hpp"
#include "genomic_interval.hpp"
#include "hash.hpp"  // for update_adler
#include "query_container.hpp"
#include "query_element.hpp"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdint>  // for std::uint32_t, std::uint64_t, std::int32_t
#include <fstream>
#include <functional>  // for std::plus
#include <iterator>    // for std::back_insert_iterator, std::cbegin
#include <numeric>     // for std::adjacent_difference, std::exclusive_scan
#include <ranges>
#include <string>
#include <tuple>  // IWYU pragma: keep
#include <vector>

namespace transferase {

[[nodiscard]] auto
genome_index_data::read(const std::string &data_file,
                        const genome_index_metadata &meta,
                        std::error_code &ec) noexcept -> genome_index_data {
  std::ifstream in(data_file, std::ios::binary);
  if (!in) {
    ec = std::make_error_code(std::errc(errno));
    return {};
  }

  std::vector<std::uint32_t> n_cpgs(meta.chrom_offset);
  n_cpgs.front() = meta.n_cpgs;
  std::ranges::rotate(n_cpgs, std::begin(n_cpgs) + 1);
  std::adjacent_difference(std::cbegin(n_cpgs), std::cend(n_cpgs),
                           std::begin(n_cpgs));

  genome_index_data data;
  for (const auto n_cpgs_chrom : n_cpgs) {
    data.positions.push_back(vec(n_cpgs_chrom));

    const auto n_bytes_expected =
      static_cast<std::streamsize>(n_cpgs_chrom * sizeof(std::uint32_t));
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    in.read(reinterpret_cast<char *>(data.positions.back().data()),
            n_bytes_expected);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto read_ok = static_cast<bool>(in);
    const auto n_bytes = in.gcount();
    if (!read_ok || n_bytes != n_bytes_expected) {
      ec = genome_index_data_error_code::failure_reading_file;
      return {};
    }
  }

  ec = std::error_code{};
  return data;
}

[[nodiscard]]
static inline auto
make_genome_index_data_filename(const std::string &dirname,
                                const std::string &genome_name) {
  const auto with_extension =
    std::format("{}{}", genome_name, genome_index_data::filename_extension);
  return (std::filesystem::path{dirname} / with_extension).string();
}

[[nodiscard]] auto
genome_index_data::read(const std::string &dirname,
                        const std::string &genome_name,
                        const genome_index_metadata &meta,
                        std::error_code &ec) noexcept -> genome_index_data {
  return read(make_genome_index_data_filename(dirname, genome_name), meta, ec);
}

[[nodiscard]] auto
genome_index_data::write(const std::string &data_file) const noexcept
  -> std::error_code {
  std::ofstream out(data_file);
  if (!out)
    return std::make_error_code(std::errc(errno));
  for (const auto &cpgs : positions) {
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    out.write(
      reinterpret_cast<const char *>(cpgs.data()),
      static_cast<std::streamsize>(sizeof(std::uint32_t) * std::size(cpgs)));
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto write_ok = static_cast<bool>(out);
    if (!write_ok)
      return std::make_error_code(std::errc(errno));
  }
  return std::error_code{};
}

// given the chromosome id (from chrom_index) and a set of ranges
// within the chrom, get the query in the form of identity of CpG
// sites using std::lower_bound
[[nodiscard]] STATIC inline auto
make_query_within_chrom(const genome_index_data::vec &positions,
                        const std::vector<chrom_range_t> &chrom_ranges) noexcept
  -> transferase::query_container {
  transferase::query_container query(std::size(chrom_ranges));
  auto cursor = std::cbegin(positions);
  // for (const auto [idx, cr] : std::views::enumerate(chrom_ranges)) {
  std::uint32_t idx = 0;
  for (const auto cr : chrom_ranges) {
    namespace rg = std::ranges;
    cursor = rg::lower_bound(cursor, std::cend(positions), cr.start);
    const auto pos_stop =
      rg::lower_bound(cursor, std::cend(positions), cr.stop);
    query[idx] = {
      static_cast<q_elem_t>(rg::distance(std::cbegin(positions), cursor)),
      static_cast<q_elem_t>(rg::distance(std::cbegin(positions), pos_stop)),
    };
    ++idx;
  }
  return query;
}

[[nodiscard]] auto
genome_index_data::make_query_chrom(
  const std::int32_t ch_id, const genome_index_metadata &meta,
  const std::vector<chrom_range_t> &chrom_ranges) const noexcept
  -> transferase::query_container {
  assert(std::ranges::is_sorted(chrom_ranges) && ch_id >= 0 &&
         ch_id < std::ranges::ssize(positions));
  const auto offset = meta.chrom_offset[ch_id];
  auto query = make_query_within_chrom(positions[ch_id], chrom_ranges);
  std::ranges::for_each(query, [&](auto &x) {
    x.start += offset;
    x.stop += offset;
  });
  return query;
}

[[nodiscard]] auto
genome_index_data::make_query(const genome_index_metadata &meta,
                              const std::vector<genomic_interval> &intervals)
  const noexcept -> transferase::query_container {
  const auto same_chrom = [](const genomic_interval &a,
                             const genomic_interval &b) {
    return a.ch_id == b.ch_id;
  };

  const auto start_stop = [](const genomic_interval &x) {
    return chrom_range_t{x.start, x.stop};
  };

  transferase::query_container query;
  query.reserve(std::size(intervals));
  for (const auto &intervals_for_chrom :
       intervals | std::views::chunk_by(same_chrom)) {
    std::vector<chrom_range_t> tmp(std::size(intervals_for_chrom));
    std::ranges::transform(intervals_for_chrom, std::begin(tmp), start_stop);
    const auto ch_id = intervals_for_chrom.front().ch_id;
    std::ranges::copy(make_query_chrom(ch_id, meta, tmp),
                      std::back_inserter(query.v));
  }
  return query;
}

[[nodiscard]] auto
genome_index_data::hash() const noexcept -> std::uint64_t {
  // ADS: plan to change this after positions is refactored into a single vec
  std::uint64_t combined = 1;  // from the zlib docs to init
  for (const auto &p : positions)
    combined =
      update_adler(combined, p.data(), std::size(p) * sizeof(genome_pos_t));
  return combined;
}

[[nodiscard]] auto
genome_index_data::get_n_cpgs() const noexcept -> std::uint32_t {
  return std::transform_reduce(std::cbegin(positions), std::cend(positions),
                               static_cast<std::uint32_t>(0), std::plus{},
                               std::size<genome_index_data::vec>);
}

[[nodiscard]] auto
genome_index_data::get_n_cpgs(const genome_index_metadata &meta,
                              const std::vector<genomic_interval> &intervals)
  const noexcept -> std::vector<std::uint32_t> {
  const auto count_cpgs = [](const auto x) -> std::uint32_t {
    return x.stop - x.start;
  };
  return std::ranges::transform_view(make_query(meta, intervals), count_cpgs) |
         std::ranges::to<std::vector>();
}

[[nodiscard]] auto
genome_index_data::get_n_cpgs(const genome_index_metadata &meta,
                              const std::uint32_t bin_size) const noexcept
  -> std::vector<std::uint32_t> {
  const auto n_bins = meta.get_n_bins(bin_size);
  std::vector<std::uint32_t> counts(n_bins);
  std::uint32_t j = 0;
  const auto zipped =
    std::views::zip(positions, meta.chrom_size, meta.chrom_offset);
  for (const auto [posn, chrom_size, offset] : zipped) {
    auto posn_itr = std::cbegin(posn);
    const auto posn_end = std::cend(posn);
    for (std::uint32_t i = 0; i < chrom_size; i += bin_size) {
      // need bin_end like this because otherwise we go into next chrom
      const auto bin_end = std::min(i + bin_size, chrom_size);
      std::uint32_t bin_count{};
      while (posn_itr != posn_end && *posn_itr < bin_end) {
        ++bin_count;
        ++posn_itr;
      }
      counts[j++] = bin_count;
    }
  }

  return counts;
}

}  // namespace transferase
