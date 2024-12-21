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

#include "cpg_index_impl.hpp"
#include "cpg_index_metadata.hpp"
#include "genomic_interval.hpp"
#include "hash.hpp"  // for update_adler
#include "xfrase_error.hpp"
#include "zlib_adapter.hpp"

#include <fcntl.h>     // for open, O_RDONLY
#include <sys/mman.h>  // for mmap, munmap, MAP_FAILED, MAP_PRIVATE
#include <unistd.h>    // for close

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <compare>  // for operator<
#include <cstddef>  // for std::size_t
#include <cstdint>  // for std::uint32_t, std::uint64_t, std::int32_t
#include <filesystem>
#include <fstream>
#include <functional>  // for std::plus
#include <iterator>    // for std::back_insert_iterator, std::cbegin
#include <numeric>     // for std::adjacent_difference, std::exclusive_scan
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>  // for std::pair, std::move
#include <vector>

[[nodiscard]] STATIC auto
mmap_genome(const std::string &filename) -> genome_file {
  const int fd = open(filename.data(), O_RDONLY, 0);
  if (fd < 0)
    return {std::make_error_code(std::errc(errno)), nullptr, 0};

  std::error_code err;
  const std::uint64_t filesize = std::filesystem::file_size(filename, err);
  if (err)
    return {std::make_error_code(std::errc(errno)), nullptr, 0};
  char *data =
    static_cast<char *>(mmap(nullptr, filesize, PROT_READ, MAP_PRIVATE, fd, 0));

  if (data == MAP_FAILED)
    return {std::make_error_code(std::errc(errno)), nullptr, 0};

  close(fd);  // close file descriptor; kernel doesn't use it

  return {std::error_code{}, data, filesize};
}

[[nodiscard]] STATIC auto
cleanup_mmap_genome(genome_file &gf) -> std::error_code {
  if (gf.data == nullptr)
    return std::make_error_code(std::errc::bad_file_descriptor);
  const int rc = munmap(static_cast<void *>(gf.data), gf.sz);
  return rc ? std::make_error_code(std::errc(errno)) : std::error_code{};
}

[[nodiscard]] STATIC auto
get_cpgs(const std::string_view chrom) -> cpg_index::vec {
  static constexpr auto expeced_max_cpg_density = 50;

  bool prev_is_c = false, curr_is_g = false;
  cpg_index::vec cpgs;
  cpgs.reserve(std::ranges::size(chrom) / expeced_max_cpg_density);

  std::uint32_t pos = 0;
  for (const auto &nuc : chrom) {
    curr_is_g = (nuc == 'g' || nuc == 'G');
    if (prev_is_c && curr_is_g)
      cpgs.push_back(pos - 1);
    prev_is_c = (nuc == 'C' || nuc == 'c' || (prev_is_c && nuc == '\n'));
    pos += (nuc != '\n');
  }
  return cpgs;
}

[[nodiscard]] STATIC auto
get_chrom_name_starts(const char *data,
                      const std::size_t sz) -> std::vector<std::size_t> {
  const auto g_end = data + sz;
  const char *g_itr = data;
  std::vector<std::size_t> starts;
  while ((g_itr = std::find(g_itr, g_end, '>')) != g_end)
    starts.push_back(std::distance(data, g_itr++));
  return starts;
}

[[nodiscard]] STATIC auto
get_chrom_name_stops(std::vector<std::size_t> starts, const char *data,
                     const std::size_t sz) -> std::vector<std::size_t> {
  const auto next_stop = [&](const std::size_t start) -> std::size_t {
    return std::distance(data, std::find(data + start, data + sz, '\n'));
  };  // finds the stop position following each start position
  std::ranges::transform(starts, std::begin(starts), next_stop);
  return starts;
}

[[nodiscard]] STATIC auto
get_chroms(const char *data, const std::size_t sz,
           const std::vector<std::size_t> &name_starts,
           const std::vector<std::size_t> &name_stops)
  -> std::vector<std::string_view> {
  assert(!name_starts.empty() && !name_stops.empty());
  std::vector<std::size_t> seq_stops(std::size(name_starts));
  seq_stops.back() = sz;

  std::ranges::copy(std::cbegin(name_starts) + 1, std::cend(name_starts),
                    std::begin(seq_stops));
  std::vector<std::size_t> seq_starts(name_stops);

  std::ranges::for_each(seq_starts, [](auto &x) { ++x; });
  std::vector<std::string_view> chroms;
  for (const auto [sta, sto] : std::views::zip(seq_starts, seq_stops))
    chroms.emplace_back(data + sta, sto - sta);

  return chroms;
}

[[nodiscard]] STATIC auto
initialize_cpg_index_plain(const std::string &genome_filename)
  -> std::tuple<cpg_index, cpg_index_metadata, std::error_code> {
  genome_file gf = mmap_genome(genome_filename);  // memory map the genome file
  if (gf.ec)
    return {{}, {}, gf.ec};

  // get start and stop positions of chrom names in the file
  const auto name_starts = get_chrom_name_starts(gf.data, gf.sz);
  const auto name_stops = get_chrom_name_stops(name_starts, gf.data, gf.sz);
  if (name_starts.empty() || name_stops.empty())
    return {{}, {}, cpg_index_code::failure_processing_genome_file};

  // initialize the chromosome order
  std::vector<std::pair<std::uint32_t, std::string>> chrom_sorter;
  for (auto idx = 0;
       const auto [start, stop] : std::views::zip(name_starts, name_stops))
    // ADS: "+1" below to skip the ">" character
    chrom_sorter.emplace_back(idx++,
                              std::string(gf.data + start + 1, gf.data + stop));

  std::ranges::sort(chrom_sorter, [](const auto &a, const auto &b) {
    return a.second < b.second;
  });

  cpg_index_metadata meta;
  meta.chrom_order =
    chrom_sorter | std::views::elements<1> | std::ranges::to<std::vector>();

  // chroms is a view into 'gf.data' so don't free gf.data too early
  auto chroms = get_chroms(gf.data, gf.sz, name_starts, name_stops);

  // order chrom sequences by the sorted order of their names
  chroms = std::views::transform(chrom_sorter | std::views::elements<0>,
                                 [&](const auto i) { return chroms[i]; }) |
           std::ranges::to<std::vector>();

  cpg_index index;

  // collect cpgs for each chrom; order must match chrom name order
  std::ranges::transform(chroms, std::back_inserter(index.positions), get_cpgs);

  // get size of each chrom to cross-check data files using this index
  std::ranges::transform(
    chroms, std::back_inserter(meta.chrom_size), [](const auto &chrom) {
      return std::ranges::size(chrom) - std::ranges::count(chrom, '\n');
    });

  // finished with any views that look into 'data' so cleanup
  if (const auto munmap_err = cleanup_mmap_genome(gf); munmap_err)
    return {{}, {}, munmap_err};

  // initialize chrom offsets within the compressed files
  std::ranges::transform(index.positions, std::back_inserter(meta.chrom_offset),
                         std::size<cpg_index::vec>);

  meta.n_cpgs =
    std::reduce(std::cbegin(meta.chrom_offset), std::cend(meta.chrom_offset));

  std::exclusive_scan(std::cbegin(meta.chrom_offset),
                      std::cend(meta.chrom_offset),
                      std::begin(meta.chrom_offset), 0);

  // init the index that maps chrom names to their rank in the order
  meta.chrom_index.clear();
  for (const auto [i, chrom_name] : std::views::enumerate(meta.chrom_order))
    meta.chrom_index.emplace(chrom_name, i);

  const auto meta_init_err = meta.init_env();
  if (meta_init_err)
    return {{}, {}, meta_init_err};

  meta.index_hash = index.hash();

  std::error_code asm_err;
  meta.assembly = get_assembly_from_filename(genome_filename, asm_err);
  if (asm_err)
    return {{}, {}, asm_err};

  return {std::move(index), std::move(meta), std::error_code{}};
}

[[nodiscard]] STATIC auto
initialize_cpg_index_gzip(const std::string &genome_filename)
  -> std::tuple<cpg_index, cpg_index_metadata, std::error_code> {
  const auto [data, gz_err] = read_gzfile_into_buffer(genome_filename);
  if (gz_err)
    return {{}, {}, cpg_index_code::failure_processing_genome_file};

  // get start and stop positions of chrom names in the file
  const auto name_starts = get_chrom_name_starts(data.data(), std::size(data));
  const auto name_stops =
    get_chrom_name_stops(name_starts, data.data(), std::size(data));
  if (name_starts.empty() || name_stops.empty())
    return {{}, {}, cpg_index_code::failure_processing_genome_file};

  // initialize the chromosome order
  std::vector<std::pair<std::uint32_t, std::string>> chrom_sorter;
  for (auto idx = 0;
       const auto [start, stop] : std::views::zip(name_starts, name_stops))
    // ADS: "+1" below to skip the ">" character
    chrom_sorter.emplace_back(
      idx++, std::string(data.data() + start + 1, data.data() + stop));

  std::ranges::sort(chrom_sorter, [](const auto &a, const auto &b) {
    return a.second < b.second;
  });

  cpg_index_metadata meta;
  meta.chrom_order =
    chrom_sorter | std::views::elements<1> | std::ranges::to<std::vector>();

  // chroms is a view into 'data.data()' so don't release data too early
  auto chroms =
    get_chroms(data.data(), std::size(data), name_starts, name_stops);

  // order chrom sequences by the sorted order of their names
  chroms = std::views::transform(chrom_sorter | std::views::elements<0>,
                                 [&](const auto i) { return chroms[i]; }) |
           std::ranges::to<std::vector>();

  cpg_index index;

  // collect cpgs for each chrom; order must match chrom name order
  std::ranges::transform(chroms, std::back_inserter(index.positions), get_cpgs);

  // get size of each chrom to cross-check data files using this index
  std::ranges::transform(
    chroms, std::back_inserter(meta.chrom_size), [](const auto &chrom) {
      return std::ranges::size(chrom) - std::ranges::count(chrom, '\n');
    });

  // initialize chrom offsets within the compressed files
  std::ranges::transform(index.positions, std::back_inserter(meta.chrom_offset),
                         std::size<cpg_index::vec>);

  meta.n_cpgs =
    std::reduce(std::cbegin(meta.chrom_offset), std::cend(meta.chrom_offset));

  std::exclusive_scan(std::cbegin(meta.chrom_offset),
                      std::cend(meta.chrom_offset),
                      std::begin(meta.chrom_offset), 0);

  // init the index that maps chrom names to their rank in the order
  meta.chrom_index.clear();
  for (const auto [i, chrom_name] : std::views::enumerate(meta.chrom_order))
    meta.chrom_index.emplace(chrom_name, i);

  const auto meta_init_err = meta.init_env();
  if (meta_init_err)
    return {{}, {}, meta_init_err};

  meta.index_hash = index.hash();

  std::error_code asm_err;
  meta.assembly = get_assembly_from_filename(genome_filename, asm_err);
  if (asm_err)
    return {{}, {}, asm_err};

  return {std::move(index), std::move(meta), std::error_code{}};
}

[[nodiscard]] auto
initialize_cpg_index(const std::string &genome_filename)
  -> std::tuple<cpg_index, cpg_index_metadata, std::error_code> {
  return is_gzip_file(genome_filename)
           ? initialize_cpg_index_gzip(genome_filename)
           : initialize_cpg_index_plain(genome_filename);
}

[[nodiscard]] auto
cpg_index::read(const cpg_index_metadata &cim, const std::string &index_file)
  -> std::tuple<cpg_index, std::error_code> {
  std::ifstream in(index_file, std::ios::binary);
  if (!in)
    return {{}, std::make_error_code(std::errc(errno))};

  std::vector<std::uint32_t> n_cpgs(cim.chrom_offset);
  {
    n_cpgs.front() = cim.n_cpgs;
    std::ranges::rotate(n_cpgs, std::begin(n_cpgs) + 1);
    std::adjacent_difference(std::cbegin(n_cpgs), std::cend(n_cpgs),
                             std::begin(n_cpgs));
  }

  cpg_index ci;
  for (const auto n_cpgs_chrom : n_cpgs) {
    ci.positions.push_back(vec(n_cpgs_chrom));
    {
      const std::streamsize n_bytes_expected =
        n_cpgs_chrom * sizeof(std::uint32_t);
      in.read(reinterpret_cast<char *>(ci.positions.back().data()),
              n_bytes_expected);
      const auto read_ok = static_cast<bool>(in);
      const auto n_bytes = in.gcount();
      if (!read_ok || n_bytes != n_bytes_expected)
        return {{},
                std::error_code(cpg_index_code::failure_reading_index_body)};
    }
  }
  return {std::move(ci), std::error_code{}};
}

[[nodiscard]] auto
cpg_index::write(const std::string &index_file) const -> std::error_code {
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

// given the chromosome id (from chrom_index) and a position within
// the chrom, get the offset of the CpG site from std::lower_bound
/* ADS: currently unused; its original use was unable to take advantage of
 * using a narrower range of search   */
// [[nodiscard]] auto
// cpg_index::get_offset_within_chrom(
//   const std::int32_t ch_id, const std::uint32_t pos) const ->
//   std::uint32_t { assert(ch_id >= 0 && ch_id <
//   std::ranges::ssize(positions)); return
//   std::ranges::distance(std::cbegin(positions[ch_id]),
//                                std::ranges::lower_bound(positions[ch_id],
//                                pos));
// }

// given the chromosome id (from chrom_index) and a position within
// the chrom, get the offset of the CpG site from std::lower_bound
[[nodiscard]] STATIC inline auto
get_offsets_within_chrom(
  const cpg_index::vec &positions,
  const std::vector<std::pair<std::uint32_t, std::uint32_t>> &queries)
  -> std::vector<std::pair<std::uint32_t, std::uint32_t>> {
  std::vector<std::pair<std::uint32_t, std::uint32_t>> res(std::size(queries));
  auto cursor = std::cbegin(positions);
  for (const auto [i, q] : std::views::enumerate(queries)) {
    cursor = std::ranges::lower_bound(cursor, std::cend(positions), q.first);
    const auto cursor_stop =
      std::ranges::lower_bound(cursor, std::cend(positions), q.second);
    res[i] = {std::ranges::distance(std::cbegin(positions), cursor),
              std::ranges::distance(std::cbegin(positions), cursor_stop)};
  }
  return res;
}

// given the chromosome id (from chrom_index) and a position within
// the chrom, get the offset of the CpG site from std::lower_bound
[[nodiscard]] auto
cpg_index::get_offsets_within_chrom(
  const std::int32_t ch_id,
  const std::vector<std::pair<std::uint32_t, std::uint32_t>> &queries) const
  -> std::vector<std::pair<std::uint32_t, std::uint32_t>> {
  assert(std::ranges::is_sorted(queries) && ch_id >= 0 &&
         ch_id < std::ranges::ssize(positions));
  return ::get_offsets_within_chrom(positions[ch_id], queries);
}

[[nodiscard]] auto
cpg_index::get_offsets(
  const std::int32_t ch_id, const cpg_index_metadata &meta,
  const std::vector<std::pair<std::uint32_t, std::uint32_t>> &queries) const
  -> std::vector<std::pair<std::uint32_t, std::uint32_t>> {
  assert(std::ranges::is_sorted(queries) && ch_id >= 0 &&
         ch_id < std::ranges::ssize(positions));

  const auto offset = meta.chrom_offset[ch_id];
  return ::get_offsets_within_chrom(positions[ch_id], queries) |
         std::views::transform(
           [&](const auto &x) -> std::pair<std::uint32_t, std::uint32_t> {
             return {offset + x.first, offset + x.second};
           }) |
         std::ranges::to<std::vector>();
}

[[nodiscard]] auto
cpg_index::get_offsets(const cpg_index_metadata &meta,
                       const std::vector<genomic_interval> &gis) const
  -> std::vector<std::pair<std::uint32_t, std::uint32_t>> {
  const auto same_chrom = [](const genomic_interval &a,
                             const genomic_interval &b) {
    return a.ch_id == b.ch_id;
  };

  const auto start_stop = [](const genomic_interval &x) {
    return std::pair<std::uint32_t, std::uint32_t>{x.start, x.stop};
  };

  std::vector<std::pair<std::uint32_t, std::uint32_t>> offsets;
  offsets.reserve(std::size(gis));
  for (const auto &gis_for_chrom : gis | std::views::chunk_by(same_chrom)) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> tmp;
    tmp.resize(std::size(gis_for_chrom));
    std::ranges::transform(gis_for_chrom, std::begin(tmp), start_stop);
    const auto ch_id = gis_for_chrom.front().ch_id;
    std::ranges::copy(get_offsets(ch_id, meta, tmp),
                      std::back_inserter(offsets));
  }
  return offsets;
}

[[nodiscard]] auto
cpg_index::hash() const -> std::uint64_t {
  // ADS: plan to change this after positions is refactored into a single vec
  std::uint64_t combined = 1;  // from the zlib docs to init
  for (const auto &p : positions)
    combined =
      update_adler(combined, p.data(), std::size(p) * sizeof(cpg_pos_t));
  return combined;
}

[[nodiscard]] auto
cpg_index::get_n_cpgs() const -> std::uint32_t {
  return std::transform_reduce(std::cbegin(positions), std::cend(positions),
                               static_cast<std::uint32_t>(0), std::plus{},
                               std::size<cpg_index::vec>);
}

[[nodiscard]] auto
read_cpg_index(const std::string &index_file,
               const std::string &index_meta_file)
  -> std::tuple<cpg_index, cpg_index_metadata, std::error_code> {
  // read the cpg_index metadata first
  const auto [cim, meta_err] = cpg_index_metadata::read(index_meta_file);
  if (meta_err)
    return {cpg_index{}, cpg_index_metadata{}, meta_err};

  // read the cpg_index using its metadata
  const auto [ci, index_err] = cpg_index::read(cim, index_file);
  if (index_err)
    return {cpg_index{}, cpg_index_metadata{}, index_err};

  return {std::move(ci), std::move(cim), {}};
}

[[nodiscard]] auto
read_cpg_index(const std::string &index_file)
  -> std::tuple<cpg_index, cpg_index_metadata, std::error_code> {
  const auto index_meta_file =
    get_default_cpg_index_metadata_filename(index_file);
  return read_cpg_index(index_file, index_meta_file);
}
