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

#ifndef R_SRC_MCLIENT_HPP_
#define R_SRC_MCLIENT_HPP_

#include "genomic_interval.hpp"
#include "methylome_client_remote.hpp"
#include "query_container.hpp"

#include <Rcpp.h>

#include <cstddef>
#include <exception>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

template <typename level_element> class MClient {
public:
  MClient();
  explicit MClient(const std::string &config_dir);

  [[nodiscard]] auto
  tostring() const -> std::string;

  [[nodiscard]] auto
  query_bins(const std::vector<std::string> &methylomes,
             const std::size_t bin_size) const -> Rcpp::NumericMatrix;

  [[nodiscard]] auto
  query_intervals(
    const std::vector<std::string> &methylomes, const std::string &genome,
    const Rcpp::DataFrame &intervals) const -> Rcpp::NumericMatrix;

  [[nodiscard]] auto
  query_preprocessed(const std::vector<std::string> &methylomes,
                     const transferase::query_container &query) const
    -> Rcpp::NumericMatrix;

  [[nodiscard]] auto
  format_query(const std::string &genome, const Rcpp::DataFrame &intervals)
    const -> transferase::query_container;

private:
  std::unique_ptr<transferase::methylome_client_remote> client{nullptr};
};

template <typename level_element>
MClient<level_element>::MClient(const std::string &config_dir) {
  try {
    client = std::make_unique<transferase::methylome_client_remote>(config_dir);
  }
  catch (const std::exception &e) {
    Rcpp::Rcerr << std::format("Error: {} (config_dir={})\n", e.what(),
                               config_dir);
  }
}

template <typename level_element>
MClient<level_element>::MClient() : MClient(std::string{}) {}

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

template <typename level_element>
[[nodiscard]] auto
MClient<level_element>::query_bins(const std::vector<std::string> &methylomes,
                                   const std::size_t bin_size) const
  -> Rcpp::NumericMatrix {
  try {
    const auto levels = client->get_levels<level_element>(methylomes, bin_size);
    return convert_to_numeric_matrix(levels);
  }
  catch (std::exception &e) {
    Rcpp::Rcerr << std::format("Error: {}\n", e.what());
    return {};
  }
}

template <typename level_element>
[[nodiscard]] auto
MClient<level_element>::format_query(const std::string &genome,
                                     const Rcpp::DataFrame &intervals) const
  -> transferase::query_container {
  try {
    std::error_code error{};
    const auto idx_itr = client->indexes->get_genome_index(genome, error);
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

    for (auto i = 0; i < intervals.rows(); ++i) {
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
    Rcpp::Rcerr << std::format("Error: {}\n", e.what());
    return {};
  }
}

template <typename level_element>
[[nodiscard]] auto
MClient<level_element>::query_preprocessed(
  const std::vector<std::string> &methylomes,
  const transferase::query_container &query) const -> Rcpp::NumericMatrix {
  try {
    const transferase::level_container_md<level_element> levels =
      client->get_levels<level_element>(methylomes, query);
    return convert_to_numeric_matrix(levels);
  }
  catch (std::exception &e) {
    Rcpp::Rcerr << std::format("Error: {}\n", e.what());
    return {};
  }
}

template <typename level_element>
[[nodiscard]] auto
MClient<level_element>::query_intervals(
  const std::vector<std::string> &methylomes, const std::string &genome,
  const Rcpp::DataFrame &intervals) const -> Rcpp::NumericMatrix {
  const auto query = format_query(genome, intervals);
  return query_preprocessed(methylomes, query);
}

template <typename level_element>
[[nodiscard]] auto
MClient<level_element>::tostring() const -> std::string {
  return std::format("MClient\n{}", client ? client->tostring() : "NA");
}

#endif  // R_SRC_MCLIENT_HPP_
