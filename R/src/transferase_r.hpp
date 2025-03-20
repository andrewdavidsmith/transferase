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

#ifndef R_SRC_TRANSFERASE_R_HPP_
#define R_SRC_TRANSFERASE_R_HPP_

#include "level_element.hpp"
#include "methylome_client_remote.hpp"
#include "query_container.hpp"

#include <Rcpp.h>

#include <cstddef>
#include <string>
#include <vector>

auto
config_xfr(const std::vector<std::string> &genomes,
           const std::string &config_dir) -> void;

auto
set_xfr_log_level(const std::string &log_level) -> void;

auto
init_logger() -> void;

[[nodiscard]] auto
get_xfr_log_level() -> std::string;

[[nodiscard]] auto
create_mclient(const std::string &config_dir)
  -> Rcpp::XPtr<transferase::methylome_client_remote>;

[[nodiscard]] auto
format_query(const Rcpp::XPtr<transferase::methylome_client_remote> client,
             const std::string &genome, const Rcpp::DataFrame intervals)
  -> Rcpp::XPtr<transferase::query_container>;

[[nodiscard]] auto
get_chrom_sizes(const Rcpp::XPtr<transferase::methylome_client_remote> client,
                const std::string &genome) -> Rcpp::DataFrame;

[[nodiscard]] auto
get_bin_names(const Rcpp::XPtr<transferase::methylome_client_remote> client,
              const std::string &genome, const std::size_t bin_size,
              const char sep) -> Rcpp::StringVector;

[[nodiscard]] auto
get_interval_names(const Rcpp::DataFrame intervals,
                   const char sep) -> Rcpp::StringVector;

[[nodiscard]] auto
get_n_cpgs(const Rcpp::XPtr<transferase::methylome_client_remote> client,
           const std::string &genome,
           const Rcpp::DataFrame intervals) -> Rcpp::NumericMatrix;

[[nodiscard]] auto
get_n_cpgs(const Rcpp::XPtr<transferase::methylome_client_remote> client,
           const std::string &genome,
           const std::uint32_t bin_size) -> Rcpp::NumericMatrix;

[[nodiscard]] auto
get_n_cpgs(const Rcpp::XPtr<transferase::query_container> query)
  -> Rcpp::NumericMatrix;

// ADS: declarations for query functions below (3 inputs x 2 outputs)

// not covered

[[nodiscard]] auto
query_bins(const Rcpp::XPtr<transferase::methylome_client_remote> client,
           const std::vector<std::string> &methylomes,
           const std::size_t bin_size) -> Rcpp::NumericMatrix;

[[nodiscard]] auto
query_preprocessed(
  const Rcpp::XPtr<transferase::methylome_client_remote> client,
  const std::vector<std::string> &methylomes,
  const Rcpp::XPtr<transferase::query_container> query) -> Rcpp::NumericMatrix;

[[nodiscard]] auto
query_intervals(const Rcpp::XPtr<transferase::methylome_client_remote> client,
                const std::vector<std::string> &methylomes,
                const std::string &genome,
                const Rcpp::DataFrame intervals) -> Rcpp::NumericMatrix;

// covered

auto
query_bins_cov(const Rcpp::XPtr<transferase::methylome_client_remote> client,
               const std::vector<std::string> &methylomes,
               const std::size_t bin_size) -> Rcpp::NumericMatrix;

auto
query_preprocessed_cov(
  const Rcpp::XPtr<transferase::methylome_client_remote> client,
  const std::vector<std::string> &methylomes,
  const Rcpp::XPtr<transferase::query_container> query) -> Rcpp::NumericMatrix;

auto
query_intervals_cov(
  const Rcpp::XPtr<transferase::methylome_client_remote> client,
  const std::vector<std::string> &methylomes, const std::string &genome,
  const Rcpp::DataFrame intervals) -> Rcpp::NumericMatrix;

#endif  // R_SRC_TRANSFERASE_R_HPP_
