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

#include "genome_index.hpp"

#include "genome_index_impl.hpp"
#include "zlib_adapter.hpp"

#include <fcntl.h>     // for open, O_RDONLY
#include <sys/mman.h>  // for mmap, munmap, MAP_FAILED, MAP_PRIVATE
#include <unistd.h>    // for close

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <compare>
#include <cstddef>  // for std::size_t
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <numeric>
#include <ranges>
#include <regex>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>  // for std::move
#include <vector>

namespace transferase {

struct genomic_interval;

auto
genome_index::write(const std::string &directory, const std::string &name,
                    std::error_code &error) const noexcept -> void {
  // make filenames
  const auto fn_wo_extn = std::filesystem::path{directory} / name;
  const auto meta_filename =
    genome_index_metadata::compose_filename(fn_wo_extn);
  const auto meta_write_ec = meta.write(meta_filename);
  if (meta_write_ec) {
    if (std::filesystem::exists(meta_filename)) {
      std::error_code remove_ec;
      [[maybe_unused]] const bool remove_ok =
        std::filesystem::remove(meta_filename, remove_ec);
      assert(remove_ok);
    }
    error = meta_write_ec;
    return;
  }
  const auto data_filename = genome_index_data::compose_filename(fn_wo_extn);
  const auto data_write_ec = data.write(data_filename);
  if (data_write_ec) {
    std::error_code remove_ec;
    if (std::filesystem::exists(data_filename)) {
      [[maybe_unused]]
      const bool remove_ok = std::filesystem::remove(meta_filename, remove_ec);
      assert(remove_ok);
    }
    if (std::filesystem::exists(meta_filename)) {
      [[maybe_unused]]
      const bool remove_ok = std::filesystem::remove(meta_filename, remove_ec);
      assert(remove_ok);
    }
  }
  error = data_write_ec;
  return;
}

[[nodiscard]] auto
genome_index::make_query(const std::vector<genomic_interval> &gis)
  const noexcept -> transferase::query_container {
  return data.make_query(meta, gis);
}

[[nodiscard]] auto
genome_index::read(const std::string &directory, const std::string &genome_name,
                   std::error_code &error) noexcept -> genome_index {
  auto meta = genome_index_metadata::read(directory, genome_name, error);
  if (error)
    return {};
  auto data = genome_index_data::read(directory, genome_name, meta, error);
  if (error)
    return {};
  return genome_index{std::move(data), std::move(meta)};
}

[[nodiscard]] auto
genome_index::files_exist(const std::string &directory,
                          const std::string &genome_index_name) noexcept
  -> bool {
  const auto fn_wo_extn = std::filesystem::path{directory} / genome_index_name;
  const auto meta_filename =
    genome_index_metadata::compose_filename(fn_wo_extn);
  const auto data_filename = genome_index_data::compose_filename(fn_wo_extn);

  std::error_code ec;
  const auto meta_exists = std::filesystem::exists(meta_filename, ec);
  if (ec || !meta_exists)
    return false;
  const auto data_exists = std::filesystem::exists(data_filename, ec);
  if (ec || !data_exists)
    return false;
  return true;
}

[[nodiscard]] auto
genome_index::parse_genome_name(
  const std::string &filename, std::error_code &error) noexcept -> std::string {
  using std::string_literals::operator""s;
  using std::literals::string_view_literals::operator""sv;
  // clang-format off
  const auto fasta_suff = std::vector {
    ".fa"sv,
    ".fa.gz"sv,
    ".faa"sv,
    ".faa.gz"sv,
    ".fasta"sv,
    ".fasta.gz"sv,
  } | std::views::join_with('|');
  // clang-format on
  const auto reference_genome_pattern =
    "("s + std::string(std::cbegin(fasta_suff), std::cend(fasta_suff)) + ")$"s;
  const std::regex suffix_re{reference_genome_pattern};
  std::smatch base_match;
  const std::string name = std::filesystem::path(filename).filename();
  if (std::regex_search(name, base_match, suffix_re)) {
    error = std::error_code{};
    return base_match.prefix().str();
  }
  error = std::make_error_code(std::errc::invalid_argument);
  return {};
}

[[nodiscard]] STATIC auto
mmap_genome(const std::string &filename) noexcept -> genome_file {
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
cleanup_mmap_genome(genome_file &gf) noexcept -> std::error_code {
  if (gf.data == nullptr)
    return std::make_error_code(std::errc::bad_file_descriptor);
  const int rc = munmap(static_cast<void *>(gf.data), gf.sz);
  return rc ? std::make_error_code(std::errc(errno)) : std::error_code{};
}

[[nodiscard]] STATIC auto
get_cpgs(const std::string_view chrom) noexcept -> genome_index_data::vec {
  static constexpr auto expeced_max_cpg_density = 50;

  bool prev_is_c = false, curr_is_g = false;
  genome_index_data::vec cpgs;
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
get_chrom_name_starts(const char *data, const std::size_t sz) noexcept
  -> std::vector<std::size_t> {
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const auto g_end = data + sz;
  const char *g_itr = data;
  std::vector<std::size_t> starts;
  while ((g_itr = std::find(g_itr, g_end, '>')) != g_end)
    starts.push_back(std::distance(data, g_itr++));
  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return starts;
}

[[nodiscard]] STATIC auto
get_chrom_name_stops(std::vector<std::size_t> starts, const char *data,
                     const std::size_t sz) noexcept
  -> std::vector<std::size_t> {
  const auto next_stop = [&](const std::size_t start) -> std::size_t {
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return std::distance(data, std::find(data + start, data + sz, '\n'));
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  };  // finds the stop position following each start position
  std::ranges::transform(starts, std::begin(starts), next_stop);
  return starts;
}

[[nodiscard]] STATIC auto
get_chroms(const char *data, const std::size_t sz,
           const std::vector<std::size_t> &name_starts,
           const std::vector<std::size_t> &name_stops) noexcept
  -> std::vector<std::string_view> {
  assert(!name_starts.empty() && !name_stops.empty());
  std::vector<std::size_t> seq_stops(std::size(name_starts));
  seq_stops.back() = sz;

  std::ranges::copy(std::cbegin(name_starts) + 1, std::cend(name_starts),
                    std::begin(seq_stops));
  std::vector<std::size_t> seq_starts(name_stops);

  std::ranges::for_each(seq_starts, [](auto &x) { ++x; });
  std::vector<std::string_view> chroms;
  for (const auto [sta, sto] : std::views::zip(seq_starts, seq_stops)) {
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    chroms.emplace_back(data + sta, sto - sta);
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

  return chroms;
}

[[nodiscard]] STATIC auto
make_genome_index_plain(const std::string &genome_filename,
                        std::error_code &error) noexcept -> genome_index {
  genome_file gf = mmap_genome(genome_filename);  // memory map the genome file
  if (gf.ec) {
    error = gf.ec;
    return {};
  }

  // get start and stop positions of chrom names in the file
  const auto name_starts = get_chrom_name_starts(gf.data, gf.sz);
  const auto name_stops = get_chrom_name_stops(name_starts, gf.data, gf.sz);
  if (name_starts.empty() || name_stops.empty()) {
    error = genome_index_error_code::failure_processing_fasta_file;
    return {};
  }

  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)

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

  genome_index_metadata meta;
  meta.chrom_order =
    chrom_sorter | std::views::elements<1> | std::ranges::to<std::vector>();

  // chroms is a view into 'gf.data' so don't free gf.data too early
  auto chroms = get_chroms(gf.data, gf.sz, name_starts, name_stops);

  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  // order chrom sequences by the sorted order of their names
  chroms = std::views::transform(chrom_sorter | std::views::elements<0>,
                                 [&](const auto i) { return chroms[i]; }) |
           std::ranges::to<std::vector>();

  genome_index_data data;

  // collect cpgs for each chrom; order must match chrom name order
  std::ranges::transform(chroms, std::back_inserter(data.positions), get_cpgs);

  // get size of each chrom to cross-check data files using this index
  std::ranges::transform(
    chroms, std::back_inserter(meta.chrom_size), [](const auto &chrom) {
      return std::ranges::size(chrom) - std::ranges::count(chrom, '\n');
    });

  // finished with any views that look into 'data' so cleanup
  error = cleanup_mmap_genome(gf);
  if (error)
    return {};

  // initialize chrom offsets within the compressed files
  std::ranges::transform(data.positions, std::back_inserter(meta.chrom_offset),
                         std::size<genome_index_data::vec>);

  meta.n_cpgs =
    std::reduce(std::cbegin(meta.chrom_offset), std::cend(meta.chrom_offset));

  std::exclusive_scan(std::cbegin(meta.chrom_offset),
                      std::cend(meta.chrom_offset),
                      std::begin(meta.chrom_offset), 0);

  // init the index that maps chrom names to their rank in the order
  meta.chrom_index.clear();
  for (const auto [i, chrom_name] : std::views::enumerate(meta.chrom_order))
    meta.chrom_index.emplace(chrom_name, i);

  error = meta.init_env();
  if (error)
    return {};

  meta.index_hash = data.hash();

  meta.assembly = genome_index::parse_genome_name(genome_filename, error);
  if (error)
    return {};

  error = std::error_code{};
  return {std::move(data), std::move(meta)};
}

[[nodiscard]] STATIC auto
make_genome_index_gzip(const std::string &genome_filename,
                       std::error_code &error) noexcept -> genome_index {
  const auto [raw, gz_err] = read_gzfile_into_buffer(genome_filename);
  if (gz_err) {
    error = genome_index_error_code::failure_processing_fasta_file;
    return {};
  }

  // get start and stop positions of chrom names in the file
  const auto name_starts = get_chrom_name_starts(raw.data(), std::size(raw));
  const auto name_stops =
    get_chrom_name_stops(name_starts, raw.data(), std::size(raw));
  if (name_starts.empty() || name_stops.empty()) {
    error = genome_index_error_code::failure_processing_fasta_file;
    return {};
  }

  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  // initialize the chromosome order
  std::vector<std::pair<std::uint32_t, std::string>> chrom_sorter;
  for (auto idx = 0;
       const auto [start, stop] : std::views::zip(name_starts, name_stops))
    // ADS: "+1" below to skip the ">" character
    chrom_sorter.emplace_back(
      idx++, std::string(raw.data() + start + 1, raw.data() + stop));

  std::ranges::sort(chrom_sorter, [](const auto &a, const auto &b) {
    return a.second < b.second;
  });

  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  genome_index_metadata meta;
  meta.chrom_order =
    chrom_sorter | std::views::elements<1> | std::ranges::to<std::vector>();

  // chroms is a view into 'raw.data()' so don't release raw too early
  auto chroms = get_chroms(raw.data(), std::size(raw), name_starts, name_stops);

  // order chrom sequences by the sorted order of their names
  chroms = std::views::transform(chrom_sorter | std::views::elements<0>,
                                 [&](const auto i) { return chroms[i]; }) |
           std::ranges::to<std::vector>();

  genome_index_data data;

  // collect cpgs for each chrom; order must match chrom name order
  std::ranges::transform(chroms, std::back_inserter(data.positions), get_cpgs);

  // get size of each chrom to cross-check data files using this index
  std::ranges::transform(
    chroms, std::back_inserter(meta.chrom_size), [](const auto &chrom) {
      return std::ranges::size(chrom) - std::ranges::count(chrom, '\n');
    });

  // initialize chrom offsets within the compressed files
  std::ranges::transform(data.positions, std::back_inserter(meta.chrom_offset),
                         std::size<genome_index_data::vec>);

  meta.n_cpgs =
    std::reduce(std::cbegin(meta.chrom_offset), std::cend(meta.chrom_offset));

  std::exclusive_scan(std::cbegin(meta.chrom_offset),
                      std::cend(meta.chrom_offset),
                      std::begin(meta.chrom_offset), 0);

  // init the index that maps chrom names to their rank in the order
  meta.chrom_index.clear();
  for (const auto [i, chrom_name] : std::views::enumerate(meta.chrom_order))
    meta.chrom_index.emplace(chrom_name, i);

  error = meta.init_env();
  if (error)
    return {};

  meta.index_hash = data.hash();

  meta.assembly = genome_index::parse_genome_name(genome_filename, error);
  if (error)
    return {};

  error = std::error_code{};
  return {std::move(data), std::move(meta)};
}

[[nodiscard]] auto
genome_index::make_genome_index(const std::string &genome_filename,
                                std::error_code &error) noexcept
  -> genome_index {
  return is_gzip_file(genome_filename)
           ? make_genome_index_gzip(genome_filename, error)
           : make_genome_index_plain(genome_filename, error);
}

[[nodiscard]] auto
genome_index::list_genome_indexes(const std::string &directory,
                                  std::error_code &error) noexcept
  -> std::vector<std::string> {
  static constexpr auto data_extn = genome_index::data_extn;
  static constexpr auto meta_extn = genome_index::meta_extn;

  using dir_itr_type = std::filesystem::directory_iterator;
  auto dir_itr = dir_itr_type(directory, error);
  if (error)
    return {};

  const auto data_files = std::ranges::subrange{dir_itr, dir_itr_type{}} |
                          std::views::filter([&](const auto &d) {
                            return d.path().extension().string() == data_extn;
                          }) |
                          std::ranges::to<std::vector>();

  const auto to_meta_file = [&](const auto &d) {
    std::string tmp = d.path().filename().string();
    return tmp.replace(tmp.find('.'), std::string::npos, meta_extn);
  };

  auto meta_names = std::ranges::transform_view(data_files, to_meta_file) |
                    std::ranges::to<std::vector>();

  std::unordered_set<std::string> meta_names_lookup;
  for (const auto &m : meta_names)
    meta_names_lookup.emplace(m);
  meta_names.clear();

  const auto remove_all_suffixes = [&](std::string s) {
    const auto dot = s.find('.');
    return dot == std::string::npos ? s : s.replace(dot, std::string::npos, "");
  };

  for (const auto &d : dir_itr_type{directory}) {
    const auto fn = d.path().filename().string();
    if (meta_names_lookup.contains(fn))
      meta_names.emplace_back(remove_all_suffixes(fn));
  }

  return meta_names;
}

}  // namespace transferase
