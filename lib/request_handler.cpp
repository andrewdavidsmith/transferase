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

#include "request_handler.hpp"

#include "genome_index.hpp"
#include "level_container.hpp"
#include "level_element.hpp"
#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_set.hpp"
#include "query_container.hpp"  // IWYU pragma: keep
#include "request.hpp"
#include "response.hpp"
#include "server_error_code.hpp"

#include <memory>  // for std::shared_ptr
#include <string>
#include <system_error>
#include <vector>

namespace transferase {

auto
request_handler::handle_request(const request &req,
                                response_header &resp_hdr) -> void {
  auto &lgr = logger::instance();
  resp_hdr = {};  // clear the response header

  // verify that the request type makes sense
  if (!req.is_valid_type()) {
    lgr.warning("Request type not valid: {}", req.request_type);
    resp_hdr.status = server_error_code::invalid_request_type;
    return;
  }

  // verify that the request type makes sense
  if (!req.is_valid_aux_value()) {
    lgr.warning("Aux value {} invalid for request type {}", req.aux_value,
                req.request_type);
    resp_hdr.status = req.is_intervals_request()
                        ? server_error_code::too_many_intervals
                        : server_error_code::bin_size_too_small;
    return;
  }

  // verify that the methylome names makes sense
  for (const auto &methylome_name : req.methylome_names)
    // cppcheck-suppress useStlAlgorithm
    if (!methylome::is_valid_name(methylome_name)) {
      lgr.warning("Malformed methylome name: {}", methylome_name);
      resp_hdr.status = server_error_code::invalid_methylome_name;
      return;
    }

  std::error_code ec;
  // get one methylome so we can associate a genome with this request
  const auto methylome_name = req.methylome_names.front();
  const auto meth = methylomes.get_methylome(methylome_name, ec);
  if (ec) {
    lgr.warning("Error reading methylome {}: {}", methylome_name, ec);
    resp_hdr.status = server_error_code::methylome_not_found;
    return;
  }

  // load the genome index (a preload)
  const auto genome_name = meth->get_genome_name();
  const auto index = indexes.get_genome_index(genome_name, ec);
  if (ec) {
    lgr.error("Failed to load genome index for {}: {}", genome_name, ec);
    resp_hdr.status = server_error_code::index_not_found;
    return;
  }

  // confirm that the methylome size is as expected
  if (req.index_hash != meth->get_index_hash()) {
    lgr.warning("Incorrect index_hash (provided={}, expected={})",
                req.index_hash, meth->get_index_hash());
    resp_hdr.status = server_error_code::invalid_index_hash;
    return;
  }

  // ensure we assign the appropriate number of rows and columns
  resp_hdr.rows = req.is_intervals_request()
                    ? req.n_intervals()
                    : index->get_n_bins(req.bin_size());
  resp_hdr.cols = req.n_methylomes();
  // at this point we don't know n_bytes yet if we have a bins request
  resp_hdr.status = server_error_code::ok;
}

template <>
auto
request_handler::intervals_get_levels<level_element_t>(
  const request &req, const query_container &query, response_header &resp_hdr,
  level_container<level_element_t> &resp_data) -> void {
  auto &lgr = logger::instance();
  std::error_code ec;
  resp_data.resize(resp_hdr.rows, resp_hdr.cols);
  auto col_itr = std::begin(resp_data);
  std::uint64_t index_hash = 0;  // check all methylomes use same genome
  for (const auto &methylome_name : req.methylome_names) {
    const auto meth = methylomes.get_methylome(methylome_name, ec);
    if (ec) {
      lgr.error("Failed to load methylome {}: {}", methylome_name, ec);
      resp_hdr.status = server_error_code::methylome_not_found;
      return;
    }
    if (index_hash == 0)
      index_hash = meth->get_index_hash();
    if (index_hash != meth->get_index_hash()) {
      lgr.warning("Inconsistent index hash values found");
      resp_hdr.status = server_error_code::inconsistent_genomes;
      return;
    }
    lgr.debug("Computing levels for methylome: {} (intervals)", methylome_name);
    meth->get_levels<level_element_t>(query, col_itr);
  }
  // we know n_bytes here (for intervals, we knew it earlier)
  resp_hdr.n_bytes = sizeof(level_element_t) * resp_hdr.rows * resp_hdr.cols;
}

template <>
auto
request_handler::intervals_get_levels<level_element_covered_t>(
  const request &req, const query_container &query, response_header &resp_hdr,
  level_container<level_element_covered_t> &resp_data) -> void {
  auto &lgr = logger::instance();
  std::error_code ec;

  resp_data.resize(resp_hdr.rows, resp_hdr.cols);
  std::uint64_t index_hash = 0;  // check all methylomes use same genome
  auto col_itr = std::begin(resp_data);
  for (const auto &methylome_name : req.methylome_names) {
    const auto meth = methylomes.get_methylome(methylome_name, ec);
    if (ec) {
      lgr.error("Failed to load methylome {}: {}", methylome_name, ec);
      resp_hdr.status = server_error_code::methylome_not_found;
      return;
    }
    if (index_hash == 0)
      index_hash = meth->get_index_hash();
    if (index_hash != meth->get_index_hash()) {
      lgr.warning("Inconsistent index hash values found");
      resp_hdr.status = server_error_code::inconsistent_genomes;
      return;
    }
    lgr.debug("Computing levels for methylome: {} (intervals, covered)",
              methylome_name);
    meth->get_levels<level_element_covered_t>(query, col_itr);
  }
  // we know n_bytes here (for intervals, we knew it earlier)
  resp_hdr.n_bytes = sizeof(level_element_t) * resp_hdr.rows * resp_hdr.cols;
}

template <>
auto
request_handler::bins_get_levels<level_element_t>(
  const request &req, response_header &resp_hdr,
  level_container<level_element_t> &resp_data) -> void {
  auto &lgr = logger::instance();
  std::error_code ec;
  std::shared_ptr<genome_index> index = nullptr;
  std::string genome_name;

  resp_data.resize(resp_hdr.rows, resp_hdr.cols);
  auto col_itr = std::begin(resp_data);
  for (const auto &methylome_name : req.methylome_names) {
    const auto meth = methylomes.get_methylome(methylome_name, ec);
    if (ec) {
      lgr.error("Failed to load methylome {}: {}", methylome_name, ec);
      resp_hdr.status = server_error_code::methylome_not_found;
      return;
    }
    // need genome index to know the bins
    if (index == nullptr) {
      genome_name = meth->get_genome_name();
      index = indexes.get_genome_index(genome_name, ec);
      if (ec) {
        lgr.error("Failed to load genome index for {}: {}", genome_name, ec);
        resp_hdr.status = server_error_code::index_not_found;
        return;
      }
    }
    else if (meth->get_genome_name() != genome_name) {
      lgr.error("Inconsistent genome names for methylomes in request "
                "(expected={}, observed={} for {})",
                genome_name, meth->get_genome_name(), methylome_name);
      resp_hdr.status = server_error_code::bad_request;
      return;
    }
    lgr.debug("Computing levels for methylome: {} (bins)", methylome_name);
    meth->get_levels<level_element_t>(req.bin_size(), *index, col_itr);
  }
  resp_hdr.n_bytes = sizeof(level_element_t) * resp_hdr.rows * resp_hdr.cols;
}

template <>
auto
request_handler::bins_get_levels<level_element_covered_t>(
  const request &req, response_header &resp_hdr,
  level_container<level_element_covered_t> &resp_data) -> void {
  auto &lgr = logger::instance();
  std::error_code ec;
  std::shared_ptr<genome_index> index = nullptr;
  std::string genome_name;

  resp_data.resize(resp_hdr.rows, resp_hdr.cols);
  auto col_itr = std::begin(resp_data);
  for (const auto &methylome_name : req.methylome_names) {
    const auto meth = methylomes.get_methylome(methylome_name, ec);
    if (ec) {
      lgr.error("Failed to load methylome {}: {}", methylome_name, ec);
      resp_hdr.status = server_error_code::methylome_not_found;
      return;
    }
    // need genome index to know the bins
    if (index == nullptr) {
      genome_name = meth->get_genome_name();
      index = indexes.get_genome_index(genome_name, ec);
      if (ec) {
        lgr.error("Failed to load genome index for {}: {}", genome_name, ec);
        resp_hdr.status = server_error_code::index_not_found;
        return;
      }
    }
    else if (meth->get_genome_name() != genome_name) {
      lgr.error("Inconsistent genome names for methylomes in request "
                "(expected={}, observed={} for {})",
                genome_name, meth->get_genome_name(), methylome_name);
      resp_hdr.status = server_error_code::bad_request;
      return;
    }
    lgr.debug("Computing levels for methylome: {} (bins, covered)",
              methylome_name);
    meth->get_levels<level_element_covered_t>(req.bin_size(), *index, col_itr);
  }
  resp_hdr.n_bytes = sizeof(level_element_t) * resp_hdr.rows * resp_hdr.cols;
}

}  // namespace transferase
