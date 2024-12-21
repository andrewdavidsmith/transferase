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

#include "genomic_interval.hpp"
#include "genomic_interval_impl.hpp"

#include "cpg_index_metadata.hpp"  // for cpg_index_metadata

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <fstream>
#include <iterator>  // for std::cend
#include <ranges>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>  // for std::move, std::pair
#include <vector>

[[nodiscard]] STATIC auto
parse(const cpg_index_metadata &cim, const std::string &line,
      std::error_code &ec) -> genomic_interval {
  auto cursor = line.data();
  const auto line_sz = std::size(line);
  const auto line_end = line.data() + line_sz;

  // Parse chromosome name
  auto first_space_pos = line.find_first_of(" \t");
  if (first_space_pos == std::string::npos) {
    ec = genomic_interval_code::error_parsing_bed_line;
    first_space_pos = line_sz;
  }
  // ADS: consider string_view here
  const std::string chrom_name(cursor, first_space_pos);
  cursor += first_space_pos + 1;

  // Parse start
  std::uint32_t start{};
  auto result = std::from_chars(std::min(cursor, line_end), line_end, start);
  if (!ec && static_cast<bool>(result.ec))
    ec = genomic_interval_code::error_parsing_bed_line;
  cursor = result.ptr + 1;

  // Parse stop
  std::uint32_t stop{};
  result = std::from_chars(std::min(cursor, line_end), line_end, stop);
  if (!ec && static_cast<bool>(result.ec))
    ec = genomic_interval_code::error_parsing_bed_line;

  // Find chromosome ID
  const auto ch_id_itr = cim.chrom_index.find(chrom_name);
  if (!ec && ch_id_itr == std::cend(cim.chrom_index))
    ec = genomic_interval_code::chrom_name_not_found_in_index;

  // Check interval
  if (!ec && stop > cim.chrom_size[ch_id_itr->second])
    ec = genomic_interval_code::interval_past_chrom_end_in_index;

  return ec ? genomic_interval{}
            : genomic_interval{ch_id_itr->second, start, stop};
}

[[nodiscard]] auto
genomic_interval::load(const cpg_index_metadata &cim, const std::string &filename)
  -> std::tuple<std::vector<genomic_interval>, std::error_code> {
  std::ifstream in{filename};
  if (!in)
    return {{}, std::make_error_code(std::errc(errno))};
  std::vector<genomic_interval> v;
  std::string line;
  std::error_code ec;
  while (getline(in, line)) {
    const auto gi = parse(cim, line, ec);
    if (ec)
      return {{}, ec};
    v.push_back(std::move(gi));
  }
  return {std::move(v), genomic_interval_code::ok};
}

[[nodiscard]] auto
intervals_sorted(const cpg_index_metadata &cim,
                 const std::vector<genomic_interval> &gis) -> bool {
  // check that chroms appear consecutively
  std::vector<std::uint32_t> chroms_seen(std::size(cim.chrom_order), 0);
  std::int32_t prev_ch_id{genomic_interval::not_a_chrom};
  for (const auto &i : gis) {
    chroms_seen[i.ch_id] += (i.ch_id != prev_ch_id);
    prev_ch_id = i.ch_id;
  }
  if (std::ranges::any_of(chroms_seen, [](const auto x) { return x > 1; }))
    return false;

  const auto same_chrom = [](const auto a, const auto b) {
    return a.ch_id == b.ch_id;
  };

  bool is_sorted = true;
  for (const auto &gis_for_chrom : gis | std::views::chunk_by(same_chrom)) {
    for (std::uint32_t prev_start{0}; const auto g : gis_for_chrom) {
      is_sorted = is_sorted && (prev_start <= g.start);
      prev_start = g.start;
    }
  }
  return is_sorted;
}
