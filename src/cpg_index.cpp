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

#include "cpg_index.hpp"
#include "genomic_interval.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifdef DEBUG
#include <iostream>
#include <print>
#endif

// for mmap
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

using std::format;
using std::formatter;
using std::ifstream;
using std::make_tuple;
using std::pair;
using std::size;
using std::size_t;
using std::string;
using std::string_view;
using std::tuple;
using std::uint32_t;
using std::vector;

namespace rg = std::ranges;
namespace vs = std::views;

struct genome_file {
  std::error_code ec{};
  char *data{};
  size_t sz{};
};

[[nodiscard]] auto
mmap_genome(const string &filename) -> genome_file {
  const int fd = open(filename.data(), O_RDONLY, 0);
  if (fd < 0)
    return {std::make_error_code(std::errc(errno)), nullptr, 0};

  std::error_code errc;
  const size_t filesize = std::filesystem::file_size(filename, errc);
  if (errc)
    return {std::make_error_code(std::errc(errno)), nullptr, 0};
  char *data =
    static_cast<char *>(mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0));

  if (data == MAP_FAILED)
    return {std::make_error_code(std::errc(errno)), nullptr, 0};

  close(fd);  // close file descriptor; kernel doesn't use it

  return {std::error_code{}, data, filesize};
}

[[nodiscard]] static auto
cleanup_mmap_genome(genome_file &gf) -> std::error_code {
  assert(gf.data != nullptr);
  const int rc = munmap(static_cast<void *>(gf.data), gf.sz);
  return rc ? std::make_error_code(std::errc(errno)) : std::error_code{};
}

[[nodiscard]] static auto
get_cpgs(const string_view &chrom) -> cpg_index::vec {
  static constexpr auto expeced_max_cpg_density = 50;

  bool prev_is_c = false, curr_is_g = false;
  cpg_index::vec cpgs;
  cpgs.reserve(rg::size(chrom) / expeced_max_cpg_density);

  uint32_t pos = 0;
  for (const auto &nuc : chrom) {
    curr_is_g = (nuc == 'g' || nuc == 'G');
    if (prev_is_c && curr_is_g)
      cpgs.push_back(pos - 1);
    prev_is_c = (nuc == 'C' || nuc == 'c' || (prev_is_c && nuc == '\n'));
    pos += (nuc != '\n');
  }
  return cpgs;
}

[[nodiscard]] static auto
get_chrom_name_starts(const char *data, const size_t sz) -> vector<size_t> {
  const auto g_end = data + sz;
  const char *g_itr = data;
  vector<size_t> starts;
  while ((g_itr = std::find(g_itr, g_end, '>')) != g_end)
    starts.push_back(std::distance(data, g_itr++));
  return starts;
}

[[nodiscard]] static auto
get_chrom_name_stops(vector<size_t> starts, const char *data,
                     const size_t sz) -> vector<size_t> {
  // copy the 'starts' vector and return it as the stops
  for (auto &s : starts)
    s = std::distance(data, std::find(data + s, data + sz, '\n'));
  return starts;
}

[[nodiscard]] static auto
get_chroms(const char *data, const size_t sz, const vector<size_t> &name_starts,
           const vector<size_t> &name_stops) -> vector<string_view> {
  vector<size_t> seq_stops(size(name_starts));
  seq_stops.back() = sz;

  rg::copy(cbegin(name_starts) + 1, cend(name_starts), begin(seq_stops));
  vector<size_t> seq_starts(name_stops);

  rg::for_each(seq_starts, [](auto &x) { ++x; });
  vector<string_view> chroms;
  for (const auto [sta, sto] : vs::zip(seq_starts, seq_stops))
    chroms.emplace_back(data + sta, sto - sta);

  return chroms;
}

[[nodiscard]] auto
cpg_index::construct(const string &genome_file) -> std::error_code {
  auto gf = mmap_genome(genome_file);  // memory map the genome file
  if (gf.ec)
    return gf.ec;

  // initialize the chromosome names and their order
  const auto name_starts = get_chrom_name_starts(gf.data, gf.sz);
  const auto name_stops = get_chrom_name_stops(name_starts, gf.data, gf.sz);
  chrom_order.clear();
  for (const auto [start, stop] : vs::zip(name_starts, name_stops))
    chrom_order.emplace_back(string_view(gf.data + start + 1, gf.data + stop));

  /* chroms is a view into 'data' so don't free 'data' too early */
  const auto chroms = get_chroms(gf.data, gf.sz, name_starts, name_stops);
  // collect cpgs for each chrom; order must match chrom name order
  positions.clear();
  for (const auto &chrom : chroms)
    positions.emplace_back(get_cpgs(chrom));

  // get the size of each chrom so we can cross-check for any data
  // files using this index
  chrom_size.clear();
  for (const auto &chrom : chroms)
    chrom_size.emplace_back(size(chrom) - rg::count(chrom, '\n'));

  // finished with views into 'data' so cleanup
  if (const auto munmap_err = cleanup_mmap_genome(gf); munmap_err)
    return munmap_err;

  // initialize chrom offsets within the compressed files
  chrom_offset.clear();
  for (const auto &cpgs : positions)
    chrom_offset.emplace_back(size(cpgs));
  n_cpgs_total = std::reduce(cbegin(chrom_offset), cend(chrom_offset));
  exclusive_scan(cbegin(chrom_offset), cend(chrom_offset), begin(chrom_offset),
                 0);

  // init the index that maps chrom names to their rank in the order
  chrom_index.clear();
  for (const auto [i, chrom_name] : vs::enumerate(chrom_order))
    chrom_index.emplace(chrom_name, i);

  return std::error_code{};
}

[[nodiscard]] auto
cpg_index::read(const string &index_file) -> std::error_code {
  // identifier is always 64 bytes
  static const auto expected_file_identifier = format("{:64}", "cpg_index");

  ifstream in(index_file);
  if (!in) {
#ifdef DEBUG
    std::println(std::cerr, "failed to open cpg index file: {}", index_file);
#endif
    return std::make_error_code(std::errc(errno));
  }

  // write the identifier so we can check it
  string file_identifier_in_file(size(expected_file_identifier), '\0');
  in.read(file_identifier_in_file.data(), size(expected_file_identifier));

  if (file_identifier_in_file != expected_file_identifier) {
#ifdef DEBUG
    std::println(std::cerr, R"(bad identifier: found "{}" expected "{}")",
                 file_identifier_in_file, expected_file_identifier);
#endif
    return std::make_error_code(std::errc(errno));
  }

  uint32_t n_header_lines{};
  {
    in.read(reinterpret_cast<char *>(&n_header_lines), sizeof(uint32_t));
    const auto read_ok = static_cast<bool>(in);
    const auto n_bytes = in.gcount();
    if (!read_ok || n_bytes != static_cast<std::streamsize>(sizeof(uint32_t)))
      return std::make_error_code(std::errc(errno));
  }

  // clear all instance variables
  chrom_order.clear();
  chrom_size.clear();
  chrom_offset.clear();
  chrom_index.clear();
  vector<uint32_t> chrom_n_cpgs;  // for convenience

  uint32_t n_lines{};
  string line;
  while (n_lines < n_header_lines && getline(in, line)) {
    std::istringstream iss(line);
    string chrom_name{};
    uint32_t chrom_sz{};
    uint32_t n_cpgs{};
    uint32_t n_preceding_cpgs{};
    if (!(iss >> chrom_name >> chrom_sz >> n_cpgs >> n_preceding_cpgs)) {
#ifdef DEBUG
      std::println(std::cerr, "failed to parse header line:\n{}", line);
#endif
      return std::make_error_code(std::errc(errno));
    }

    chrom_order.emplace_back(chrom_name);
    chrom_size.push_back(chrom_sz);
    chrom_offset.push_back(n_preceding_cpgs);
    chrom_n_cpgs.push_back(n_cpgs);
    chrom_index.emplace(chrom_name, n_lines);
    ++n_lines;
  }

  if (n_lines != n_header_lines) {
#ifdef DEBUG
    std::println(std::cerr, "failed to parse header: {}", index_file);
#endif
    return std::make_error_code(std::errc(errno));
  }
  n_cpgs_total = std::reduce(cbegin(chrom_n_cpgs), cend(chrom_n_cpgs));

  positions.clear();
  for (const auto n_cpgs : chrom_n_cpgs) {
    positions.push_back(vec(n_cpgs));
    {
      const std::streamsize n_bytes_expected = n_cpgs * sizeof(uint32_t);
      in.read(reinterpret_cast<char *>(positions.back().data()),
              n_bytes_expected);
      const auto read_ok = static_cast<bool>(in);
      const auto n_bytes = in.gcount();
      if (!read_ok || n_bytes != n_bytes_expected)
        return std::make_error_code(std::errc(errno));
    }
  }

  return std::error_code{};
}

[[nodiscard]] auto
cpg_index::write(const string &index_file) const -> std::error_code {
  // identifier is always 64 bytes
  static const auto file_identifier = format("{:64}", "cpg_index");

  string header;
  // write the chrom offsets and number of cpgs in chrom name order
  // the chrom_index will be implicit in the header
  for (const auto [i, chrom_name] : vs::enumerate(chrom_order)) {
    const auto n_cpgs = size(positions[i]);
    const auto n_preceding_cpgs = chrom_offset[i];
    const auto chrom_sz = chrom_size[i];
    header += format("{}\t{}\t{}\t{}\n", chrom_name, chrom_sz, n_cpgs,
                     n_preceding_cpgs);
  }

  std::ofstream out(index_file.data());
  if (!out) {
#ifdef DEBUG
    std::println(std::cerr, "failed to open index file: {}", index_file);
#endif
    return std::make_error_code(std::errc(errno));
  }

  {
    // write the identifier so we can check it
    out.write(file_identifier.data(), size(file_identifier));
    const auto write_ok = static_cast<bool>(out);
    if (!write_ok)
      return std::make_error_code(std::errc(errno));
  }

  {
    // write the number of header lines so we know how much to read
    const uint32_t n_header_lines = size(chrom_order);
    out.write(reinterpret_cast<const char *>(&n_header_lines),
              sizeof(uint32_t));
    const auto write_ok = static_cast<bool>(out);
    if (!write_ok)
      return std::make_error_code(std::errc(errno));
  }

  for (const auto &cpgs : positions) {
    out.write(reinterpret_cast<const char *>(cpgs.data()),
              sizeof(uint32_t) * size(cpgs));
    const auto write_ok = static_cast<bool>(out);
    if (!write_ok)
      return std::make_error_code(std::errc(errno));
  }

  return std::error_code{};
}

[[nodiscard]] auto
cpg_index::tostring() const -> string {
  // clang-format off
  static constexpr auto fmt_tmplt = "{}\t{}\t{}\t{}\t{}\n";
  static constexpr auto header =
    "idx"
    "\t"
    "chrom"
    "\t"
    "size "
    "\t"
    "cpgs"
    "\t"
    "offset"
    "\n"
    ;
  // clang-format on
  string res{header};
  int32_t chrom_counter = 0;
  const auto zipped = vs::zip(chrom_order, positions, chrom_size, chrom_offset);
  for (const auto [nm, ps, sz, of] : zipped) {
    const auto idx_itr = chrom_index.find(nm);
    if (idx_itr == cend(chrom_index))
      res += format("{} missing\n", nm);
    else
      res += format(fmt_tmplt, idx_itr->second, nm, sz, size(ps), of);
    if (chrom_counter != idx_itr->second)
      res += format("{} wrong order: {}\n", nm, idx_itr->second);
    ++chrom_counter;
  }
  res += format("n_cpgs_total: {}", n_cpgs_total);
  return res;
}

// given the chromosome id (from chrom_index) and a position within
// the chrom, get the offset of the CpG site from std::lower_bound
auto
cpg_index::get_offset_within_chrom(const int32_t ch_id,
                                   const uint32_t pos) const -> uint32_t {
  assert(ch_id >= 0 && ch_id < ssize(positions));
  return rg::distance(cbegin(positions[ch_id]),
                      rg::lower_bound(positions[ch_id], pos));
}

// given the chromosome id (from chrom_index) and a position within
// the chrom, get the offset of the CpG site from std::lower_bound
[[nodiscard]] static inline auto
get_offsets_within_chrom(const cpg_index::vec &positions,
                         const vector<pair<uint32_t, uint32_t>> &queries)
  -> vector<pair<uint32_t, uint32_t>> {
  vector<pair<uint32_t, uint32_t>> res(size(queries));
  auto cursor = cbegin(positions);
  for (const auto [i, q] : vs::enumerate(queries)) {
    cursor = rg::lower_bound(cursor, cend(positions), q.first);
    const auto cursor_stop = rg::lower_bound(cursor, cend(positions), q.second);
    res[i] = {rg::distance(cbegin(positions), cursor),
              rg::distance(cbegin(positions), cursor_stop)};
  }
  return res;
}

// given the chromosome id (from chrom_index) and a position within
// the chrom, get the offset of the CpG site from std::lower_bound
[[nodiscard]] auto
cpg_index::get_offsets_within_chrom(
  const int32_t ch_id, const vector<pair<uint32_t, uint32_t>> &queries) const
  -> vector<pair<uint32_t, uint32_t>> {
  assert(rg::is_sorted(queries) && ch_id >= 0 && ch_id < ssize(positions));
  return ::get_offsets_within_chrom(positions[ch_id], queries);
}

[[nodiscard]] auto
cpg_index::get_offsets(const int32_t ch_id,
                       const vector<pair<uint32_t, uint32_t>> &queries) const
  -> vector<pair<uint32_t, uint32_t>> {
  assert(rg::is_sorted(queries) && ch_id >= 0 && ch_id < ssize(positions));

  const auto offset = chrom_offset[ch_id];
  return ::get_offsets_within_chrom(positions[ch_id], queries) |
         vs::transform([&](const auto &x) -> pair<uint32_t, uint32_t> {
           return {offset + x.first, offset + x.second};
         }) |
         rg::to<vector>();
}

[[nodiscard]] auto
cpg_index::get_offsets(const vector<genomic_interval> &gis) const
  -> vector<pair<uint32_t, uint32_t>> {
  assert(rg::is_sorted(gis));

  const auto same_chrom = [](const genomic_interval &a,
                             const genomic_interval &b) {
    return a.ch_id == b.ch_id;
  };
  vector<pair<uint32_t, uint32_t>> offsets;
  offsets.reserve(size(gis));
  for (const auto &gis_for_chrom : gis | vs::chunk_by(same_chrom)) {
    vector<pair<uint32_t, uint32_t>> tmp;
    tmp.reserve(size(gis_for_chrom));
    for (const auto &gi : gis_for_chrom)
      tmp.emplace_back(gi.start, gi.stop);
    const auto ch_id = gis_for_chrom.front().ch_id;
    rg::copy(get_offsets(ch_id, tmp), back_inserter(offsets));
  }
  return offsets;
}
