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
#include "mxe_error.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <numeric>  // std::exclusive_scan
#include <ranges>
#include <regex>  // used in parsing filenames
#include <string>
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

struct genome_file {
  std::error_code ec{};
  char *data{};
  std::size_t sz{};
};

[[nodiscard]] auto
mmap_genome(const std::string &filename) -> genome_file {
  const int fd = open(filename.data(), O_RDONLY, 0);
  if (fd < 0)
    return {std::make_error_code(std::errc(errno)), nullptr, 0};

  std::error_code err;
  const std::uint64_t filesize = std::filesystem::file_size(filename, err);
  if (err)
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
get_cpgs(const std::string_view &chrom) -> cpg_index::vec {
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

[[nodiscard]] static auto
get_chrom_name_starts(const char *data,
                      const std::size_t sz) -> std::vector<std::size_t> {
  const auto g_end = data + sz;
  const char *g_itr = data;
  std::vector<std::size_t> starts;
  while ((g_itr = std::find(g_itr, g_end, '>')) != g_end)
    starts.push_back(std::distance(data, g_itr++));
  return starts;
}

[[nodiscard]] static auto
get_chrom_name_stops(std::vector<std::size_t> starts, const char *data,
                     const std::size_t sz) -> std::vector<std::size_t> {
  const auto next_stop = [&](const std::size_t start) -> std::size_t {
    return std::distance(data, std::find(data + start, data + sz, '\n'));
  };  // finds the stop position following each start position
  std::ranges::transform(starts, std::begin(starts), next_stop);
  return starts;
}

[[nodiscard]] static auto
get_chroms(const char *data, const std::size_t sz,
           const std::vector<std::size_t> &name_starts,
           const std::vector<std::size_t> &name_stops)
  -> std::vector<std::string_view> {
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

[[nodiscard]] auto
cpg_index::construct(const std::string &genome_file) -> std::error_code {
  auto gf = mmap_genome(genome_file);  // memory map the genome file
  if (gf.ec)
    return gf.ec;

  // get start and stop positions of chrom names in the file
  const auto name_starts = get_chrom_name_starts(gf.data, gf.sz);
  const auto name_stops = get_chrom_name_stops(name_starts, gf.data, gf.sz);

  // initialize the chromosome order
  chrom_order.clear();
  for (const auto [start, stop] : std::views::zip(name_starts, name_stops))
    // ADS: "+1" below to skip the ">" character
    chrom_order.emplace_back(
      std::string_view(gf.data + start + 1, gf.data + stop));

  const auto n_chroms = std::size(chrom_order);

  // chroms is a view into 'gf.data' so don't free gf.data too early
  const auto chroms = get_chroms(gf.data, gf.sz, name_starts, name_stops);

  // collect cpgs for each chrom; order must match chrom name order
  positions.resize(n_chroms);
  std::ranges::transform(chroms, std::begin(positions), get_cpgs);

  // get size of each chrom to cross-check data files using this index
  chrom_size.resize(n_chroms);
  std::ranges::transform(chroms, std::begin(chrom_size), [](const auto &chrom) {
    return std::ranges::size(chrom) - std::ranges::count(chrom, '\n');
  });

  // finished with any views that look into 'data' so cleanup
  if (const auto munmap_err = cleanup_mmap_genome(gf); munmap_err)
    return munmap_err;

  // initialize chrom offsets within the compressed files
  chrom_offset.resize(n_chroms);
  std::ranges::transform(positions, std::begin(chrom_offset),
                         std::size<cpg_index::vec>);

  n_cpgs_total =
    std::reduce(std::cbegin(chrom_offset), std::cend(chrom_offset));

  std::exclusive_scan(std::cbegin(chrom_offset), std::cend(chrom_offset),
                      std::begin(chrom_offset), 0);

  // init the index that maps chrom names to their rank in the order
  chrom_index.clear();
  for (const auto [i, chrom_name] : std::views::enumerate(chrom_order))
    chrom_index.emplace(chrom_name, i);

  return std::error_code{};
}

[[nodiscard]] auto
cpg_index::read(const std::string &index_file) -> std::error_code {
  // identifier is always 64 bytes
  static const auto expected_file_identifier =
    std::format("{:64}", "cpg_index");

  std::ifstream in(index_file, std::ios::binary);
  if (!in)
    return std::make_error_code(std::errc(errno));

  // read the identifier so we can check it
  auto file_identifier_in_file =
    std::string(std::size(expected_file_identifier), '\0');
  in.read(file_identifier_in_file.data(), std::size(expected_file_identifier));

  if (file_identifier_in_file != expected_file_identifier) {
#ifdef DEBUG
    std::println(R"(Bad identifier: found "{}" expected "{}")",
                 file_identifier_in_file, expected_file_identifier);
#endif
    return cpg_index_code::wrong_identifier_in_header;
  }

  std::uint32_t n_header_lines{};
  {
    in.read(reinterpret_cast<char *>(&n_header_lines), sizeof(std::uint32_t));
    const auto read_ok = static_cast<bool>(in);
    const auto n_bytes = in.gcount();
    if (!read_ok ||
        n_bytes != static_cast<std::streamsize>(sizeof(std::uint32_t)))
      return std::make_error_code(std::errc(errno));
  }

  // clear all instance variables
  chrom_order.clear();
  chrom_size.clear();
  chrom_offset.clear();
  chrom_index.clear();
  std::vector<std::uint32_t> chrom_n_cpgs;  // for convenience

  std::uint32_t n_lines{};
  std::string line;
  while (n_lines < n_header_lines && getline(in, line)) {
    std::istringstream iss(line);
    std::string chrom_name{};
    std::uint32_t chrom_sz{};
    std::uint32_t n_cpgs{};
    std::uint32_t n_preceding_cpgs{};
    if (!(iss >> chrom_name >> chrom_sz >> n_cpgs >> n_preceding_cpgs)) {
#ifdef DEBUG
      std::println("Failed to parse header line:\n{}", line);
#endif
      return cpg_index_code::error_parsing_index_header_line;
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
    std::println("Failed to parse header: {}", index_file);
#endif
    return cpg_index_code::failure_reading_index_header;
  }

  n_cpgs_total =
    std::reduce(std::cbegin(chrom_n_cpgs), std::cend(chrom_n_cpgs));

  positions.clear();
  for (const auto n_cpgs : chrom_n_cpgs) {
    positions.push_back(vec(n_cpgs));
    {
      const std::streamsize n_bytes_expected = n_cpgs * sizeof(std::uint32_t);
      in.read(reinterpret_cast<char *>(positions.back().data()),
              n_bytes_expected);
      const auto read_ok = static_cast<bool>(in);
      const auto n_bytes = in.gcount();
      if (!read_ok || n_bytes != n_bytes_expected)
        return cpg_index_code::failure_reading_index_body;
    }
  }
  return std::error_code{};
}

[[nodiscard]] auto
cpg_index::write(const std::string &index_file) const -> std::error_code {
  // identifier is always 64 bytes
  static const auto file_identifier = std::format("{:64}", "cpg_index");

  std::string header;
  // write the chrom offsets and number of cpgs in chrom name order
  // the chrom_index will be implicit in the header
  for (const auto [i, chrom_name] : std::views::enumerate(chrom_order)) {
    const auto n_cpgs = std::size(positions[i]);
    const auto n_preceding_cpgs = chrom_offset[i];
    const auto chrom_sz = chrom_size[i];
    header += std::format("{}\t{}\t{}\t{}\n", chrom_name, chrom_sz, n_cpgs,
                          n_preceding_cpgs);
  }

  std::ofstream out(index_file);
  if (!out) {
#ifdef DEBUG
    std::println("Failed to open index file: {}", index_file);
#endif
    return std::make_error_code(std::errc(errno));
  }

  {
    // write the identifier so we can check it
    out.write(file_identifier.data(), std::size(file_identifier));
    const auto write_ok = static_cast<bool>(out);
    if (!write_ok)
      return std::make_error_code(std::errc(errno));
  }

  {
    // write the number of header lines so we know how much to read
    // (must be 4-byte value)
    const std::uint32_t n_header_lines = std::size(chrom_order);
    out.write(reinterpret_cast<const char *>(&n_header_lines),
              sizeof(std::uint32_t));
    const auto write_ok = static_cast<bool>(out);
    if (!write_ok)
      return std::make_error_code(std::errc(errno));
  }

  {
    // write the header itself
    out.write(header.data(), std::size(header));
    const auto write_ok = static_cast<bool>(out);
    if (!write_ok)
      return std::make_error_code(std::errc(errno));
  }

  for (const auto &cpgs : positions) {
    out.write(reinterpret_cast<const char *>(cpgs.data()),
              sizeof(std::uint32_t) * std::size(cpgs));
    const auto write_ok = static_cast<bool>(out);
    if (!write_ok)
      return std::make_error_code(std::errc(errno));
  }

  return std::error_code{};
}

[[nodiscard]] auto
cpg_index::tostring() const -> std::string {
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
  std::string res{header};
  std::int32_t chrom_counter = 0;
  const auto zipped =
    std::views::zip(chrom_order, positions, chrom_size, chrom_offset);
  for (const auto [nm, ps, sz, of] : zipped) {
    const auto idx_itr = chrom_index.find(nm);
    if (idx_itr == std::cend(chrom_index))
      res += std::format("{} missing\n", nm);
    else
      res += std::format(fmt_tmplt, idx_itr->second, nm, sz, std::size(ps), of);
    if (chrom_counter != idx_itr->second)
      res += std::format("{} wrong order: {}\n", nm, idx_itr->second);
    ++chrom_counter;
  }
  res += std::format("n_cpgs_total: {}", n_cpgs_total);
  return res;
}

// given the chromosome id (from chrom_index) and a position within
// the chrom, get the offset of the CpG site from std::lower_bound
/* ADS: currently unused; its original use was unable to take advantage of using
 * a narrower range of search   */
// [[nodiscard]] auto
// cpg_index::get_offset_within_chrom(
//   const std::int32_t ch_id, const std::uint32_t pos) const -> std::uint32_t {
//   assert(ch_id >= 0 && ch_id < std::ranges::ssize(positions));
//   return std::ranges::distance(std::cbegin(positions[ch_id]),
//                                std::ranges::lower_bound(positions[ch_id],
//                                pos));
// }

// given the chromosome id (from chrom_index) and a position within
// the chrom, get the offset of the CpG site from std::lower_bound
[[nodiscard]] static inline auto
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
  const std::int32_t ch_id,
  const std::vector<std::pair<std::uint32_t, std::uint32_t>> &queries) const
  -> std::vector<std::pair<std::uint32_t, std::uint32_t>> {
  assert(std::ranges::is_sorted(queries) && ch_id >= 0 &&
         ch_id < std::ranges::ssize(positions));

  const auto offset = chrom_offset[ch_id];
  return ::get_offsets_within_chrom(positions[ch_id], queries) |
         std::views::transform(
           [&](const auto &x) -> std::pair<std::uint32_t, std::uint32_t> {
             return {offset + x.first, offset + x.second};
           }) |
         std::ranges::to<std::vector>();
}

[[nodiscard]] auto
cpg_index::get_offsets(const std::vector<genomic_interval> &gis) const
  -> std::vector<std::pair<std::uint32_t, std::uint32_t>> {
  assert(std::ranges::is_sorted(gis));

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
    std::ranges::copy(get_offsets(ch_id, tmp), std::back_inserter(offsets));
  }
  return offsets;
}

[[nodiscard]] auto
cpg_index::get_n_bins(const std::uint32_t bin_size) const -> std::uint32_t {
  const auto get_n_bins_for_chrom = [&](const auto chrom_size) {
    return (chrom_size + bin_size) / bin_size;
  };
  return std::transform_reduce(std::cbegin(chrom_size), std::cend(chrom_size),
                               static_cast<std::uint32_t>(0), std::plus{},
                               get_n_bins_for_chrom);
}

[[nodiscard]] auto
get_assembly_from_filename(const std::string &filename,
                           std::error_code &ec) -> std::string {
  // ADS: this regular expression might better be in a header
  static constexpr auto assembly_ptrn = R"(^[_[:alnum:]]+)";
  static const auto filename_ptrn =
    std::format(R"({}.{}$)", assembly_ptrn, cpg_index::filename_extension);

  const std::string name = std::filesystem::path(filename).filename();

  std::regex filename_re(filename_ptrn);
  if (std::regex_search(name, filename_re)) {
    std::smatch base_match;
    std::regex assembly_re(assembly_ptrn);
    if (std::regex_search(name, base_match, assembly_re))
      return base_match[0].str();
  }
  ec = std::make_error_code(std::errc::invalid_argument);
  return {};
}
