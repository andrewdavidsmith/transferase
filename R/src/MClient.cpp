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

#include "MClient.hpp"

#include "genomic_interval.hpp"
#include "level_container_md.hpp"
#include "methylome_client_remote.hpp"

#include <Rcpp.h>

#include <exception>
#include <format>
#include <memory>
#include <string>
#include <vector>

MClient::MClient(const std::string &config_dir) {
  try {
    client = std::make_unique<transferase::methylome_client_remote>(config_dir);
  }
  catch (const std::exception &e) {
    Rcpp::Rcerr << std::format("Error: {} (config_dir={})\n", e.what(),
                               config_dir);
  }
}

MClient::MClient() {
  try {
    client =
      std::make_unique<transferase::methylome_client_remote>(std::string{});
  }
  catch (const std::exception &e) {
    Rcpp::Rcerr << std::format("Error: {}\n", e.what());
  }
}

auto
MClient::query_bins(const std::vector<std::string> &methylomes,
                    const std::size_t bin_size) const -> Rcpp::NumericMatrix {
  try {
    const transferase::level_container_md<transferase::level_element_t> levels =
      client->get_levels<transferase::level_element_t>(methylomes, bin_size);

    Rcpp::NumericMatrix m(levels.n_rows, levels.n_cols);
    for (auto i = 0u; i < levels.n_rows; ++i)
      for (auto j = 0u; j < levels.n_cols; ++j)
        m(i, j) = levels[i, j].get_wmean();
    return m;
  }
  catch (std::exception &e) {
    Rcpp::Rcerr << std::format("Error: {}\n", e.what());
    return {};
  }
}

auto
MClient::query_intervals(const std::vector<std::string> &methylomes,
                         Rcpp::NumericMatrix intervals) const
  -> Rcpp::NumericMatrix {
  try {
    std::vector<transferase::genomic_interval> q(intervals.rows());
    for (auto i = 0; i < intervals.rows(); ++i) {
      q[i].ch_id = intervals(i, 0);
      q[i].start = intervals(i, 1);
      q[i].stop = intervals(i, 2);
    }
    const transferase::level_container_md<transferase::level_element_t> levels =
      client->get_levels<transferase::level_element_t>(methylomes, q);
    Rcpp::NumericMatrix m(levels.n_rows, levels.n_cols);
    for (auto i = 0u; i < levels.n_rows; ++i)
      for (auto j = 0u; j < levels.n_cols; ++j)
        m(i, j) = levels[i, j].get_wmean();
    return m;
  }
  catch (std::exception &e) {
    Rcpp::Rcerr << std::format("Error: {}\n", e.what());
    return {};
  }
}

auto
MClient::tostring() const -> std::string {
  return client == nullptr ? "NA" : client->tostring();
}
