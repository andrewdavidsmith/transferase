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

#include "methylome.hpp"

#include <zlib.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>
#include <utility>  // std::move
#include <vector>

using std::error_code;
using std::pair;
using std::size;
using std::string;
using std::uint16_t;
using std::uint32_t;
using std::uint8_t;
using std::vector;

namespace rg = std::ranges;
namespace vs = std::views;

template <typename T>
[[nodiscard]] static inline auto
compress(const T &in, vector<uint8_t> &out) -> int {
  z_stream strm{};
  int ret = deflateInit2(&strm, Z_BEST_SPEED, Z_DEFLATED, MAX_WBITS,
                         MAX_MEM_LEVEL, Z_RLE);
  if (ret != Z_OK)
    return ret;

  // pointer to bytes to compress
  // ADS: 'next_in' is 'z_const' defined as 'const' in zconf.h
  strm.next_in = reinterpret_cast<uint8_t *>(
    const_cast<typename T::value_type *>(in.data()));
  // bytes available to compress
  const auto n_input_bytes = size(in) * sizeof(typename T::value_type);
  strm.avail_in = n_input_bytes;

  out.resize(deflateBound(&strm, n_input_bytes));

  strm.next_out = out.data();  // pointer to bytes for compressed data
  strm.avail_out = size(out);  // bytes available for compressed data

  ret = deflate(&strm, Z_FINISH);
  assert(ret != Z_STREAM_ERROR);

  const int ret_end = deflateEnd(&strm);
  ret = ret_end ? ret_end : ret;
  assert(ret_end != Z_STREAM_ERROR);

  out.resize(strm.total_out);

  return ret;
}

template <typename T>
[[nodiscard]] static inline auto
decompress(vector<uint8_t> &in, T &out) -> int {
  z_stream strm{};
  int ret = inflateInit(&strm);
  if (ret != Z_OK)
    return ret;

  strm.next_in = in.data();  // pointer to compressed bytes
  strm.avail_in = size(in);  // bytes available to decompress

  // pointer to bytes for decompressed data
  strm.next_out = reinterpret_cast<uint8_t *>(out.data());
  // bytes available for decompressed data
  const auto n_output_bytes = size(out) * sizeof(typename T::value_type);
  strm.avail_out = n_output_bytes;

  ret = inflate(&strm, Z_FINISH);
  assert(ret != Z_STREAM_ERROR);

  switch (ret) {
  case Z_NEED_DICT:
    ret = Z_DATA_ERROR;  // fall through
  case Z_DATA_ERROR:
  case Z_MEM_ERROR:
    inflateEnd(&strm);
    return ret;
  }

  const int ret_end = inflateEnd(&strm);
  assert(ret_end != Z_STREAM_ERROR);
  ret = ret_end ? ret_end : ret;

  // assume size(out) is an exact fit
  // out.resize(strm.total_out);

  return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

[[nodiscard]] auto
methylome::read(const string &filename, const uint32_t n_cpgs) -> int {
  error_code errc;
  const auto filesize = std::filesystem::file_size(filename, errc);
  if (errc)
    return -1;
  std::ifstream in(filename);
  if (!in)
    return -1;
  uint64_t flags{};
  if (!in.read(reinterpret_cast<char *>(&flags), sizeof(flags)))
    return -1;
  if (flags == 1 && n_cpgs == 0)
    return -1;  // can't know how much to inflate
  const auto datasize = filesize - sizeof(flags);
  if (flags == 1) {
    vector<uint8_t> buf(datasize);
    if (!in.read(reinterpret_cast<char *>(buf.data()), datasize))
      return -1;
    cpgs.resize(n_cpgs);
    return decompress(buf, cpgs);
  }
  cpgs.resize(datasize / record_size);
  in.read(reinterpret_cast<char *>(cpgs.data()), datasize);
  return in ? 0 : -1;
}

[[nodiscard]] auto
methylome::write(const string &filename, const bool zip) const -> int {
  vector<uint8_t> buf;
  if (zip && compress(cpgs, buf))
    return -1;

  std::ofstream out(filename);
  if (!out)
    return -1;
  const uint64_t flags = zip;
  if (!out.write(reinterpret_cast<const char *>(&flags), sizeof(flags)))
    return -1;
  if (zip) {
    if (!out.write(reinterpret_cast<const char *>(buf.data()), size(buf)))
      return -1;
  }
  else {
    if (!out.write(reinterpret_cast<const char *>(cpgs.data()),
                   size(cpgs) * record_size))
      return -1;
  }
  return 0;
}

auto
methylome::operator+=(const methylome &rhs) -> methylome & {
  assert(size(cpgs) == size(rhs.cpgs));
  rg::transform(cpgs, rhs.cpgs, begin(cpgs),
                [](const auto &l, const auto &r) -> m_elem {
                  return {l.first + r.first, l.second + r.second};
                });
  return *this;
}

[[nodiscard]] auto
methylome::get_counts(const cpg_index::vec &positions, const uint32_t offset,
                      const uint32_t start,
                      const uint32_t stop) const -> counts_res {
  const auto cpg_beg_lb = rg::lower_bound(positions, start);
  const auto cpg_beg =
    cbegin(cpgs) + offset + rg::distance(cbegin(positions), cpg_beg_lb);
  const auto cpg_end =
    cbegin(cpgs) + offset +
    rg::distance(cbegin(positions),
                 rg::lower_bound(cpg_beg_lb, cend(positions), stop));
  uint32_t n_meth{};
  uint32_t n_unmeth{};
  uint32_t n_sites_covered{};
  for (auto cursor = cpg_beg; cursor != cpg_end; ++cursor) {
    n_meth += cursor->first;
    n_unmeth += cursor->second;
    n_sites_covered += (cursor->first + cursor->second > 0);
  }
  return {n_meth, n_unmeth, n_sites_covered};
}

template <typename T>
[[nodiscard]] static inline auto
get_counts_impl(const T b, const T e) -> counts_res {
  counts_res t;
  for (auto cursor = b; cursor != e; ++cursor) {
    t.n_meth += cursor->first;
    t.n_unmeth += cursor->second;
    t.n_covered += *cursor != pair<uint16_t, uint16_t>{};
  }
  return t;
}

[[nodiscard]] auto
methylome::get_counts(const uint32_t start,
                      const uint32_t stop) const -> counts_res {
  return get_counts_impl(cbegin(cpgs) + start, cbegin(cpgs) + stop);
}

[[nodiscard]] auto
methylome::get_counts(const vector<pair<uint32_t, uint32_t>> &queries) const
  -> vector<counts_res> {
  vector<counts_res> res(size(queries));
  const auto cpg_beg = cbegin(cpgs);
  for (const auto [i, q] : vs::enumerate(queries))
    res[i] = get_counts_impl(cpg_beg + q.first, cpg_beg + q.second);
  return res;
}

[[nodiscard]] auto
methylome::total_counts() const -> counts_res {
  uint32_t n_meth{};
  uint32_t n_unmeth{};
  uint32_t n_covered{};
  for (const auto &cpg : cpgs) {
    n_meth += cpg.first;
    n_unmeth += cpg.second;
    n_covered += cpg != pair<uint16_t, uint16_t>{};
  }
  return {n_meth, n_unmeth, n_covered};
}

static auto
bin_counts(cpg_index::vec::const_iterator &posn_itr,
           const cpg_index::vec::const_iterator posn_end,
           const uint32_t bin_end, methylome::vec::const_iterator &cpg_itr)
  -> counts_res {
  uint32_t n_meth{};
  uint32_t n_unmeth{};
  uint32_t n_covered{};
  while (posn_itr != posn_end && *posn_itr < bin_end) {
    n_meth += cpg_itr->first;
    n_unmeth += cpg_itr->second;
    n_covered += (cpg_itr->first + cpg_itr->second > 0);
    ++cpg_itr;
    ++posn_itr;
  }
  return {n_meth, n_unmeth, n_covered};
}

[[nodiscard]] auto
methylome::get_bins(const uint32_t bin_size,
                    const cpg_index &index) const -> vector<counts_res> {

  // ADS TODO: reserve n_bins
  vector<counts_res> results;

  string chrom_name;
  const auto zipped =
    vs::zip(index.positions, index.chrom_size, index.chrom_offset);
  for (const auto [positions, chrom_size, offset] : zipped) {
    auto posn_itr = cbegin(positions);
    const auto posn_end = cend(positions);
    auto cpg_itr = cbegin(cpgs) + offset;
    for (uint32_t i = 0; i < chrom_size; i += bin_size) {
      const auto bin_end = std::min(i + bin_size, chrom_size);
      results.emplace_back(bin_counts(posn_itr, posn_end, bin_end, cpg_itr));
    }
  }
  return results;
}
