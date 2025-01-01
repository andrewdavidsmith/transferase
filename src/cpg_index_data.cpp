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

#include "cpg_index_data.hpp"

#include "cpg_index_data_impl.hpp"
#include "cpg_index_metadata.hpp"
#include "cpg_index_types.hpp"
#include "genomic_interval.hpp"
#include "hash.hpp"  // for update_adler

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
#include <utility>  // for std::pair, std::move
#include <vector>

namespace xfrase {

[[nodiscard]] auto
cpg_index_data::read(const std::string &index_file,
                     const cpg_index_metadata &meta,
                     std::error_code &ec) -> cpg_index_data {
  std::ifstream in(index_file, std::ios::binary);
  if (!in) {
    ec = std::make_error_code(std::errc(errno));
    return {};
  }

  std::vector<std::uint32_t> n_cpgs(meta.chrom_offset);
  {
    n_cpgs.front() = meta.n_cpgs;
    std::ranges::rotate(n_cpgs, std::begin(n_cpgs) + 1);
    std::adjacent_difference(std::cbegin(n_cpgs), std::cend(n_cpgs),
                             std::begin(n_cpgs));
  }

  cpg_index_data data;
  for (const auto n_cpgs_chrom : n_cpgs) {
    data.positions.push_back(vec(n_cpgs_chrom));
    {
      const std::streamsize n_bytes_expected =
        n_cpgs_chrom * sizeof(std::uint32_t);
      in.read(reinterpret_cast<char *>(data.positions.back().data()),
              n_bytes_expected);
      const auto read_ok = static_cast<bool>(in);
      const auto n_bytes = in.gcount();
      if (!read_ok || n_bytes != n_bytes_expected) {
        ec = std::error_code(cpg_index_data_code::failure_reading_index_data);
        return {};
      }
    }
  }

  ec = std::error_code{};
  return data;
}

[[nodiscard]]
static inline auto
make_cpg_index_data_filename(const std::string &dirname,
                             const std::string &genomic_name) {
  const auto with_extension =
    std::format("{}{}", genomic_name, cpg_index_data::filename_extension);
  return (std::filesystem::path{dirname} / with_extension).string();
}

[[nodiscard]] auto
cpg_index_data::read(const std::string &dirname,
                     const std::string &genomic_name,
                     const cpg_index_metadata &meta,
                     std::error_code &ec) -> cpg_index_data {
  return read(make_cpg_index_data_filename(dirname, genomic_name), meta, ec);
}

[[nodiscard]] auto
cpg_index_data::write(const std::string &index_file) const -> std::error_code {
  std::ofstream out(index_file);
  if (!out)
    return std::make_error_code(std::errc(errno));
  for (const auto &cpgs : positions) {
    out.write(reinterpret_cast<const char *>(cpgs.data()),
              sizeof(std::uint32_t) * std::size(cpgs));
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
make_query_within_chrom(const cpg_index_data::vec &positions,
                        const std::vector<chrom_range_t> &chrom_ranges)
  -> xfrase::query {
  xfrase::query qry(std::size(chrom_ranges));
  auto cursor = std::cbegin(positions);
  for (const auto [i, q] : std::views::enumerate(chrom_ranges)) {
    cursor = std::ranges::lower_bound(cursor, std::cend(positions), q.first);
    const auto cursor_stop =
      std::ranges::lower_bound(cursor, std::cend(positions), q.second);
    qry[i] = {std::ranges::distance(std::cbegin(positions), cursor),
              std::ranges::distance(std::cbegin(positions), cursor_stop)};
  }
  return qry;
}

// given the chromosome id (from chrom_index) and a set of ranges
// within the chrom, get the query in the form of CpG site identities
[[nodiscard]] auto
cpg_index_data::make_query_within_chrom(
  const std::int32_t ch_id,
  const std::vector<chrom_range_t> &chrom_ranges) const -> xfrase::query {
  assert(std::ranges::is_sorted(chrom_ranges) && ch_id >= 0 &&
         ch_id < std::ranges::ssize(positions));
  return xfrase::make_query_within_chrom(positions[ch_id], chrom_ranges);
}

[[nodiscard]] auto
cpg_index_data::make_query_chrom(
  const std::int32_t ch_id, const cpg_index_metadata &meta,
  const std::vector<chrom_range_t> &chrom_ranges) const -> xfrase::query {
  assert(std::ranges::is_sorted(chrom_ranges) && ch_id >= 0 &&
         ch_id < std::ranges::ssize(positions));

  const auto offset = meta.chrom_offset[ch_id];

  auto q = xfrase::make_query_within_chrom(positions[ch_id], chrom_ranges) |
           std::views::transform([&](const auto &x) -> xfrase::query_element {
             return {offset + x.first, offset + x.second};
           }) |
           std::ranges::to<std::vector>();
  return xfrase::query{std::move(q)};
}

[[nodiscard]] auto
cpg_index_data::make_query(const cpg_index_metadata &meta,
                           const std::vector<genomic_interval> &gis) const
  -> xfrase::query {
  const auto same_chrom = [](const genomic_interval &a,
                             const genomic_interval &b) {
    return a.ch_id == b.ch_id;
  };

  const auto start_stop = [](const genomic_interval &x) {
    return chrom_range_t{x.start, x.stop};
  };

  xfrase::query qry;
  qry.reserve(std::size(gis));
  for (const auto &gis_for_chrom : gis | std::views::chunk_by(same_chrom)) {
    std::vector<chrom_range_t> tmp(std::size(gis_for_chrom));
    std::ranges::transform(gis_for_chrom, std::begin(tmp), start_stop);
    const auto ch_id = gis_for_chrom.front().ch_id;
    std::ranges::copy(make_query_chrom(ch_id, meta, tmp),
                      std::back_inserter(qry.v));
  }
  return qry;
}

[[nodiscard]] auto
cpg_index_data::hash() const -> std::uint64_t {
  // ADS: plan to change this after positions is refactored into a single vec
  std::uint64_t combined = 1;  // from the zlib docs to init
  for (const auto &p : positions)
    combined =
      update_adler(combined, p.data(), std::size(p) * sizeof(cpg_pos_t));
  return combined;
}

[[nodiscard]] auto
cpg_index_data::get_n_cpgs() const -> std::uint32_t {
  return std::transform_reduce(std::cbegin(positions), std::cend(positions),
                               static_cast<std::uint32_t>(0), std::plus{},
                               std::size<cpg_index_data::vec>);
}

}  // namespace xfrase
