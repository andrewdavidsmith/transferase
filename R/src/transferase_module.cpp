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

#include "genome_index.hpp"
#include "genomic_interval.hpp"
#include "logger.hpp"

#include <Rcpp.h>

#include <ranges>
#include <string>
#include <vector>

RCPP_EXPOSED_CLASS_NODECL(transferase::genomic_interval)
RCPP_EXPOSED_CLASS_NODECL(transferase::genome_index)

auto
read_genomic_interval(const transferase::genome_index &index,
                      const std::string &filename) -> Rcpp::NumericMatrix {
  static constexpr auto n_cols = 3u;
  const auto intervals = transferase::genomic_interval::read(index, filename);
  const auto n_rows = std::size(intervals);
  Rcpp::NumericMatrix m(n_rows, n_cols);
  for (auto row_id = 0u; row_id < n_rows; ++row_id) {
    m(row_id, 0) = intervals[row_id].ch_id;
    m(row_id, 1) = intervals[row_id].start;
    m(row_id, 2) = intervals[row_id].stop;
  }
  return m;
}

auto
read_genomic_interval(const std::string &dirname,
                      const std::string &genome) -> transferase::genome_index {
  try {
    return transferase::genome_index::read(dirname, genome);
  }
  catch (const std::exception &e) {
    Rcpp::Rcout << std::format("{}\n", e.what());
    return {};
  }
}

RCPP_MODULE(transferase) {
  using namespace Rcpp;

  namespace xfr = transferase;

  [[maybe_unused]] auto &lgr =
    xfr::logger::instance(std::make_shared<std::ostream>(Rcpp::Rcout.rdbuf()),
                          "transferase", xfr::log_level_t::error);

  Rcpp::class_<MClient>("MClient")
    .constructor()
    .constructor<std::string>()
    .method("tostring", &MClient::tostring, "documentation")
    .method("query_bins", &MClient::query_bins, "documentation")
    .method("query_intervals", &MClient::query_intervals, "documentation")
    //
    ;

  Rcpp::class_<xfr::genomic_interval>("GenomicInterval")
    .constructor()
    .field("ch_id", &xfr::genomic_interval::ch_id, "documentation")
    .field("start", &xfr::genomic_interval::start, "documentation")
    .field("stop", &xfr::genomic_interval::stop, "documentation")
    //
    ;

  Rcpp::class_<xfr::genome_index>("GenomeIndex")
    .constructor()
    .method("tostring", &xfr::genome_index::tostring, "documentation")
    //
    ;

  function("read_genomic_interval", &read_genomic_interval, "documentation");
  function("read_genome_index", &read_genomic_interval, "documentation");
}
