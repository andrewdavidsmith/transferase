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
configXfr(const std::vector<std::string> &genomes,
          const std::string &config_dir) -> void;

auto
setXfrLogLevel(const std::string &log_level) -> void;

auto
initLogger() -> void;

[[nodiscard]] auto
getXfrLogLevel() -> std::string;

[[nodiscard]] auto
createMClient(const std::string &config_dir)
  -> Rcpp::XPtr<transferase::methylome_client_remote>;

auto
formatQuery(const Rcpp::XPtr<transferase::methylome_client_remote> client,
            const std::string &genome, const Rcpp::DataFrame intervals)
  -> Rcpp::XPtr<transferase::query_container>;

// ADS: declarations for query functions below (3 inputs x 2 outputs)

// not covered

[[nodiscard]] auto
queryBins(const Rcpp::XPtr<transferase::methylome_client_remote> client,
          const std::vector<std::string> &methylomes,
          const std::size_t bin_size) -> Rcpp::NumericMatrix;

[[nodiscard]] auto
queryPreprocessed(const Rcpp::XPtr<transferase::methylome_client_remote> client,
                  const std::vector<std::string> &methylomes,
                  const Rcpp::XPtr<transferase::query_container> query)
  -> Rcpp::NumericMatrix;

[[nodiscard]] auto
queryIntervals(const Rcpp::XPtr<transferase::methylome_client_remote> client,
               const std::vector<std::string> &methylomes,
               const std::string &genome,
               const Rcpp::DataFrame intervals) -> Rcpp::NumericMatrix;

// covered

auto
queryBinsCov(const Rcpp::XPtr<transferase::methylome_client_remote> client,
             const std::vector<std::string> &methylomes,
             const std::size_t bin_size) -> Rcpp::NumericMatrix;

auto
queryPreprocessedCov(
  const Rcpp::XPtr<transferase::methylome_client_remote> client,
  const std::vector<std::string> &methylomes,
  const Rcpp::XPtr<transferase::query_container> query) -> Rcpp::NumericMatrix;

auto
queryIntervalsCov(const Rcpp::XPtr<transferase::methylome_client_remote> client,
                  const std::vector<std::string> &methylomes,
                  const std::string &genome,
                  const Rcpp::DataFrame intervals) -> Rcpp::NumericMatrix;

#endif  // R_SRC_TRANSFERASE_R_HPP_
