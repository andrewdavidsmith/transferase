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

#include "utilities.hpp"

/*
  Functions defined here are used by multiple source files
 */

#include "cpg_index.hpp"
#include "genomic_interval.hpp"
#include "methylome.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <ostream>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

using std::array;
using std::pair;
using std::string;
using std::vector;

namespace rg = std::ranges;
namespace vs = std::views;

auto
write_intervals(std::ostream &out, const cpg_index &index,
                const vector<genomic_interval> &gis,
                const vector<counts_res> &results) -> void {
  static constexpr auto buf_size{512};
  static constexpr auto delim{'\t'};

  array<char, buf_size> buf{};
  const auto buf_end = buf.data() + buf_size;

  using gis_res = std::pair<const genomic_interval &, const counts_res &>;
  const auto same_chrom = [](const gis_res &a, const gis_res &b) {
    return a.first.ch_id == b.first.ch_id;
  };

  for (const auto &chunk : vs::zip(gis, results) | vs::chunk_by(same_chrom)) {
    const auto ch_id = get<0>(chunk.front()).ch_id;
    const string chrom{index.chrom_order[ch_id]};
    rg::copy(chrom, buf.data());
    buf[size(chrom)] = delim;
    for (const auto &[gi, res] : chunk) {
      std::to_chars_result tcr{buf.data() + size(chrom) + 1, std::errc()};
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wstringop-overflow=0"
      tcr = std::to_chars(tcr.ptr, buf_end, gi.start);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, gi.stop);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, res.n_meth);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, res.n_unmeth);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, res.n_covered);
      *tcr.ptr++ = '\n';
#pragma GCC diagnostic push
      out.write(buf.data(), rg::distance(buf.data(), tcr.ptr));
    }
  }
}
auto
write_bins(std::ostream &out, const uint32_t bin_size, const cpg_index &index,
           const vector<counts_res> &results) -> void {
  static constexpr auto buf_size{512};
  static constexpr auto delim{'\t'};

  array<char, buf_size> buf{};
  const auto buf_beg = buf.data();
  const auto buf_end = buf.data() + buf_size;

  vector<counts_res>::const_iterator res{cbegin(results)};

  const auto zipped = vs::zip(index.chrom_size, index.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    rg::copy(chrom_name, buf_beg);
    buf[size(chrom_name)] = delim;
    for (uint32_t bin_beg = 0; bin_beg < chrom_size; bin_beg += bin_size) {
      const auto bin_end = std::min(bin_beg + bin_size, chrom_size);
      std::to_chars_result tcr{buf_beg + size(chrom_name) + 1, std::errc()};
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wstringop-overflow=0"
      tcr = std::to_chars(tcr.ptr, buf_end, bin_beg);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, bin_end);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, res->n_meth);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, res->n_unmeth);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, res->n_covered);
      *tcr.ptr++ = '\n';
#pragma GCC diagnostic push
      out.write(buf_beg, rg::distance(buf_beg, tcr.ptr));
      ++res;
    }
  }
  assert(res == cend(results));
}
