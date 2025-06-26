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
#include "query_container.hpp"
#include "remote_client.hpp"

#include <Rcpp.h>

#include <cstddef>
#include <string>
#include <vector>

// [[Rcpp::export]]
void
config_xfr(const std::vector<std::string> &genomes,
           const std::string &config_dir);

// [[Rcpp::export]]
void
set_xfr_log_level(const std::string &log_level);

// [[Rcpp::export]]
void
init_logger();

// [[Rcpp::export]]
std::string
get_xfr_log_level();

// [[Rcpp::export]]
Rcpp::XPtr<transferase::remote_client>
create_mclient(const std::string &config_dir);

// [[Rcpp::export]]
Rcpp::XPtr<transferase::query_container>
format_query(const Rcpp::XPtr<transferase::remote_client> client,
             const std::string &genome, const Rcpp::DataFrame intervals);

// [[Rcpp::export]]
Rcpp::DataFrame
get_chrom_sizes(const Rcpp::XPtr<transferase::remote_client> client,
                const std::string &genome);

// [[Rcpp::export]]
Rcpp::StringVector
get_bin_names(const Rcpp::XPtr<transferase::remote_client> client,
              const std::string &genome, const std::size_t bin_size,
              const char sep);

// [[Rcpp::export]]
Rcpp::StringVector
get_interval_names(const Rcpp::DataFrame intervals, const char sep);

// [[Rcpp::export]]
Rcpp::NumericMatrix
get_n_cpgs(const Rcpp::XPtr<transferase::remote_client> client,
           const std::string &genome, const Rcpp::DataFrame intervals);

// [[Rcpp::export]]
Rcpp::NumericMatrix
get_n_cpgs_bins(const Rcpp::XPtr<transferase::remote_client> client,
                const std::string &genome, const std::uint32_t bin_size);

// [[Rcpp::export]]
Rcpp::NumericMatrix
get_n_cpgs_query(const Rcpp::XPtr<transferase::query_container> query);

// [[Rcpp::export]]
Rcpp::NumericMatrix
get_wmeans(const Rcpp::NumericMatrix m, const bool has_n_covered,
           const std::uint32_t min_count);

// ADS: declarations for query functions below (3 inputs x 2 outputs)

// not covered

// [[Rcpp::export]]
Rcpp::NumericMatrix
query_bins(const Rcpp::XPtr<transferase::remote_client> client,
           const std::string &genome,
           const std::vector<std::string> &methylomes,
           const std::size_t bin_size);

// [[Rcpp::export]]
Rcpp::NumericMatrix
query_windows(const Rcpp::XPtr<transferase::remote_client> client,
              const std::string &genome,
              const std::vector<std::string> &methylomes,
              const std::size_t window_size, const std::size_t window_step);

// [[Rcpp::export]]
Rcpp::NumericMatrix
query_preprocessed(const Rcpp::XPtr<transferase::remote_client> client,
                   const std::string &genome,
                   const std::vector<std::string> &methylomes,
                   const Rcpp::XPtr<transferase::query_container> query);

// [[Rcpp::export]]
Rcpp::NumericMatrix
query_intervals(const Rcpp::XPtr<transferase::remote_client> client,
                const std::string &genome,
                const std::vector<std::string> &methylomes,
                const Rcpp::DataFrame intervals);

// covered

// [[Rcpp::export]]
Rcpp::NumericMatrix
query_bins_cov(const Rcpp::XPtr<transferase::remote_client> client,
               const std::string &genome,
               const std::vector<std::string> &methylomes,
               const std::size_t bin_size);

// [[Rcpp::export]]
Rcpp::NumericMatrix
query_windows_cov(const Rcpp::XPtr<transferase::remote_client> client,
                  const std::string &genome,
                  const std::vector<std::string> &methylomes,
                  const std::size_t window_size, const std::size_t window_step);

// [[Rcpp::export]]
Rcpp::NumericMatrix
query_preprocessed_cov(const Rcpp::XPtr<transferase::remote_client> client,
                       const std::string &genome,
                       const std::vector<std::string> &methylomes,
                       const Rcpp::XPtr<transferase::query_container> query);

// [[Rcpp::export]]
Rcpp::NumericMatrix
query_intervals_cov(const Rcpp::XPtr<transferase::remote_client> client,
                    const std::string &genome,
                    const std::vector<std::string> &methylomes,
                    const Rcpp::DataFrame intervals);

#endif  // R_SRC_TRANSFERASE_R_HPP_
