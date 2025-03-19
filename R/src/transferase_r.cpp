/* MIT License
 *
 * Copyright (c) 2025 Andrew D Smith
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

#include "transferase_r.hpp"

#include "genomic_interval.hpp"
#include "level_element.hpp"
#include "logger.hpp"
#include "methylome_client_remote.hpp"
#include "query_container.hpp"
#include "system_config.hpp"  // for get_system_config_filename

#include <Rcpp.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

[[nodiscard]] static inline auto
find_dir(const std::vector<std::string> &paths,
         const std::string &filename) -> std::string {
  static constexpr auto msg_fmt = "Failed to locate system config file: {}";
  for (const auto &p : paths) {
    // Some of the paths given by the environment might not exist
    const auto path_elem_exists = std::filesystem::exists(p);
    if (!path_elem_exists)
      continue;
    const auto dir_itr = std::filesystem::recursive_directory_iterator(p);
    for (const auto &dir_entry : dir_itr) {
      const auto &curr_dir = dir_entry.path();
      if (std::filesystem::is_directory(curr_dir)) {
        const auto candidate = curr_dir / filename;
        if (std::filesystem::exists(candidate))
          return curr_dir.string();
      }
    }
  }
  throw std::runtime_error(std::format(msg_fmt, filename));
}

[[nodiscard]] static inline auto
get_package_paths() -> std::vector<std::string> {
  static constexpr auto raw_lib_paths = ".libPaths";
  const Rcpp::Function lib_paths_fun(raw_lib_paths);
  const auto lib_paths = lib_paths_fun();
  std::vector<std::string> results;
  const auto n_paths = LENGTH(lib_paths);
  for (const auto i : std::views::iota(0, n_paths)) {
    const auto curr_path = STRING_PTR(lib_paths)[i];
    results.emplace_back(CHAR(curr_path), LENGTH(curr_path));
  }
  return results;
}

[[nodiscard]] static inline auto
find_R_sys_config_dir() -> std::string {
  const auto sys_conf_file = transferase::get_system_config_filename();
  const auto package_paths = get_package_paths();
  return find_dir(package_paths, sys_conf_file);
}

auto
config_xfr(const std::vector<std::string> &genomes,
           const std::string &config_dir = "") -> void {
  const auto download_policy = transferase::download_policy_t::update;
  const auto sys_config_dir = find_R_sys_config_dir();
  transferase::client_config cfg(config_dir, sys_config_dir);
  cfg.install(genomes, download_policy, sys_config_dir);
}

auto
set_xfr_log_level(const std::string &log_level) -> void {
  const auto itr = transferase::str_to_level.find(log_level);
  if (itr == std::cend(transferase::str_to_level))
    throw std::runtime_error(std::format("Invalid log level: {}\n", log_level));
  transferase::logger::set_level(itr->second);
}

auto
get_xfr_log_level() -> std::string {
  return transferase::logger::get_level();
}

auto
create_mclient(const std::string &config_dir = "")
  -> Rcpp::XPtr<transferase::methylome_client_remote> {
  typedef transferase::methylome_client_remote MClient;
  auto client = std::make_unique<MClient>(config_dir);
  return Rcpp::XPtr<MClient>(client.release());
}

[[nodiscard]] auto
format_query_impl(const transferase::methylome_client_remote &client,
                  const std::string &genome, const Rcpp::DataFrame intervals)
  -> transferase::query_container {
  try {
    std::error_code error{};
    const auto idx_itr = client.indexes->get_genome_index(genome, error);
    if (error) {
      const auto msg = std::format("(check that {} is installed)", genome);
      throw std::system_error(error, msg);
    }
    const auto &index = *idx_itr;
    const auto &chrom_index = index.get_metadata().chrom_index;

    std::vector<transferase::genomic_interval> query(intervals.rows());
    std::string prev_chrom;
    std::int32_t ch_id{};
    std::unordered_set<std::int32_t> chroms_seen;

    const Rcpp::CharacterVector chroms = intervals[0];
    const Rcpp::IntegerVector starts = intervals[1];
    const Rcpp::IntegerVector stops = intervals[2];

    for (const auto i : std::views::iota(0, intervals.rows())) {
      const std::string chrom = Rcpp::as<std::string>(chroms[i]);
      if (chrom != prev_chrom) {
        const auto ch_id_itr = chrom_index.find(chrom);
        if (ch_id_itr == std::cend(chrom_index)) {
          const auto msg = std::format("failed to find chrom: {}", chrom);
          throw std::system_error(error, msg);
        }
        ch_id = ch_id_itr->second;
        if (chroms_seen.contains(ch_id)) {
          const auto msg = std::format("chroms unsorted ({} at {})", chrom, i);
          throw std::runtime_error(msg);
        }
        chroms_seen.insert(ch_id);
        prev_chrom = chrom;
      }
      query[i].ch_id = ch_id;
      query[i].start = starts[i];
      query[i].stop = stops[i];
    }
    return index.make_query(query);
  }
  catch (std::exception &e) {
    Rcpp::Rcerr << std::format("failed formatting query: {}\n", e.what());
    return {};
  }
}

auto
format_query(const Rcpp::XPtr<transferase::methylome_client_remote> client,
             const std::string &genome, const Rcpp::DataFrame intervals)
  -> Rcpp::XPtr<transferase::query_container> {
  typedef transferase::query_container qct;
  auto q = std::make_unique<qct>(format_query_impl(*client, genome, intervals));
  return Rcpp::XPtr<qct>(q.release());
}

auto
init_logger() -> void {
  [[maybe_unused]] auto &lgr = transferase::logger::instance(
    std::make_shared<std::ostream>(Rcpp::Rcout.rdbuf()), "transferase",
    transferase::log_level_t::error);
}

[[nodiscard]] static inline auto
convert_to_numeric_matrix(
  const transferase::level_container_md<transferase::level_element_t> &levels)
  -> Rcpp::NumericMatrix {
  Rcpp::NumericMatrix m(levels.n_rows, 2 * levels.n_cols);
  for (const auto row_id : std::views::iota(0u, levels.n_rows))
    for (const auto col_id : std::views::iota(0u, levels.n_cols)) {
      m(row_id, 2 * col_id) = levels[row_id, col_id].n_meth;
      m(row_id, 2 * col_id + 1) = levels[row_id, col_id].n_unmeth;
    }
  return m;
}

auto
query_bins(const Rcpp::XPtr<transferase::methylome_client_remote> client,
           const std::vector<std::string> &methylomes,
           const std::size_t bin_size) -> Rcpp::NumericMatrix {
  const auto levels =
    client->get_levels<transferase::level_element_t>(methylomes, bin_size);
  return convert_to_numeric_matrix(levels);
}

auto
query_preprocessed(
  const Rcpp::XPtr<transferase::methylome_client_remote> client,
  const std::vector<std::string> &methylomes,
  const Rcpp::XPtr<transferase::query_container> query) -> Rcpp::NumericMatrix {
  const auto levels =
    client->get_levels<transferase::level_element_t>(methylomes, *query);
  return convert_to_numeric_matrix(levels);
}

auto
query_intervals(const Rcpp::XPtr<transferase::methylome_client_remote> client,
                const std::vector<std::string> &methylomes,
                const std::string &genome,
                const Rcpp::DataFrame intervals) -> Rcpp::NumericMatrix {
  const auto query = format_query_impl(*client, genome, intervals);
  const auto levels =
    client->get_levels<transferase::level_element_t>(methylomes, query);
  return convert_to_numeric_matrix(levels);
}

[[nodiscard]] static inline auto
convert_to_numeric_matrix(
  const transferase::level_container_md<transferase::level_element_covered_t>
    &levels) -> Rcpp::NumericMatrix {
  Rcpp::NumericMatrix m(levels.n_rows, 3 * levels.n_cols);
  for (const auto row_id : std::views::iota(0u, levels.n_rows))
    for (const auto col_id : std::views::iota(0u, levels.n_cols)) {
      m(row_id, 3 * col_id) = levels[row_id, col_id].n_meth;
      m(row_id, 3 * col_id + 1) = levels[row_id, col_id].n_unmeth;
      m(row_id, 3 * col_id + 2) = levels[row_id, col_id].n_covered;
    }
  return m;
}

auto
query_bins_cov(const Rcpp::XPtr<transferase::methylome_client_remote> client,
               const std::vector<std::string> &methylomes,
               const std::size_t bin_size) -> Rcpp::NumericMatrix {
  const auto levels = client->get_levels<transferase::level_element_covered_t>(
    methylomes, bin_size);
  return convert_to_numeric_matrix(levels);
}

auto
query_preprocessed_cov(
  const Rcpp::XPtr<transferase::methylome_client_remote> client,
  const std::vector<std::string> &methylomes,
  const Rcpp::XPtr<transferase::query_container> query) -> Rcpp::NumericMatrix {
  const auto levels = client->get_levels<transferase::level_element_covered_t>(
    methylomes, *query);
  return convert_to_numeric_matrix(levels);
}

auto
query_intervals_cov(
  const Rcpp::XPtr<transferase::methylome_client_remote> client,
  const std::vector<std::string> &methylomes, const std::string &genome,
  const Rcpp::DataFrame intervals) -> Rcpp::NumericMatrix {
  const auto query = format_query_impl(*client, genome, intervals);
  const auto levels =
    client->get_levels<transferase::level_element_covered_t>(methylomes, query);
  return convert_to_numeric_matrix(levels);
}

[[nodiscard]] auto
get_chrom_sizes(const Rcpp::XPtr<transferase::methylome_client_remote> client,
                const std::string &genome) -> Rcpp::DataFrame {
  std::error_code error;
  const auto idx_itr = client->indexes->get_genome_index(genome, error);
  if (error) {
    const auto msg = std::format("(check that {} is installed)", genome);
    throw std::system_error(error, msg);
  }
  const auto &meta = idx_itr->get_metadata();
  auto names = meta.chrom_order;
  auto sizes = meta.chrom_size;
  return Rcpp::DataFrame::create(Rcpp::Named("name") = std::move(names),
                                 Rcpp::Named("size") = std::move(sizes));
}

[[nodiscard]] auto
get_bin_names(const Rcpp::XPtr<transferase::methylome_client_remote> client,
              const std::string &genome,
              const std::size_t bin_size) -> Rcpp::StringVector {
  static constexpr auto total_buf_size{128};
  static constexpr auto buf_size{total_buf_size - 10};  // allow max digits
  static constexpr auto sep{'_'};

  std::array<char, total_buf_size> buf{};
  const auto buf_beg = buf.data();
  const auto buf_end = buf_beg + buf_size;

  std::error_code error;
  const auto idx_itr = client->indexes->get_genome_index(genome, error);
  if (error) {
    const auto msg = std::format("(check that {} is installed)", genome);
    throw std::system_error(error, msg);
  }
  const auto &meta = idx_itr->get_metadata();
  const auto n_bins = meta.get_n_bins(bin_size);

  Rcpp::StringVector names(n_bins);
  std::size_t total_bin_count = 0;

  // ADS: not checking for errors in here...
  const auto zipped = std::views::zip(meta.chrom_order, meta.chrom_size);
  for (const auto [chrom_name, chrom_size] : zipped) {
    auto cursor = std::ranges::copy(chrom_name, buf_beg).out;
    *cursor++ = sep;
    for (std::uint32_t i = 0; i < chrom_size; i += bin_size) {
      auto tcr = std::to_chars(cursor, buf_end, i);
      names(total_bin_count++) = std::string(buf_beg, tcr.ptr);
    }
  }
  return names;
}

[[nodiscard]] auto
get_interval_names(
  const Rcpp::XPtr<transferase::methylome_client_remote> client,
  const Rcpp::DataFrame intervals) -> Rcpp::StringVector {
  static constexpr auto total_buf_size{128};
  static constexpr auto buf_size{total_buf_size - 10};  // allow max digits
  static constexpr auto sep{'_'};

  std::array<char, total_buf_size> buf{};
  const auto buf_beg = buf.data();
  const auto buf_end = buf_beg + buf_size;

  const Rcpp::CharacterVector chroms = intervals[0];
  const Rcpp::IntegerVector starts = intervals[1];
  const Rcpp::IntegerVector stops = intervals[2];

  const auto n_intervals = intervals.rows();
  Rcpp::StringVector names(n_intervals);

  std::string prev_name;
  auto cursor = buf_beg;
  for (auto i = 0u; i < n_intervals; ++i) {
    const auto &curr_name = Rcpp::as<std::string>(chroms[i]);
    if (prev_name != curr_name) {
      cursor = std::ranges::copy(curr_name, buf_beg).out;
      *cursor++ = sep;
      prev_name = curr_name;
    }
    auto tcr = std::to_chars(cursor, buf_end, starts[i]);
    *tcr.ptr++ = sep;
    tcr = std::to_chars(tcr.ptr, buf_end, stops[i]);
    names(i) = std::string(buf_beg, tcr.ptr);
  }
  return names;
}
