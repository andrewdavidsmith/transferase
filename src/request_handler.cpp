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
#include "server.hpp"

#include <memory>  // for std::shared_ptr
#include <string>
#include <system_error>
#include <vector>

namespace transferase {

auto
request_handler::handle_request(const request &req,
                                response_header &resp_hdr) -> void {
  auto &lgr = logger::instance();
  resp_hdr.rows = 0;
  resp_hdr.cols = 0;

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
    resp_hdr.status = server_error_code::invalid_aux_value;
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

  resp_hdr.rows = req.is_intervals_request()
                    ? req.n_intervals()
                    : index->get_n_bins(req.bin_size());
  resp_hdr.cols = req.n_methylomes();
  resp_hdr.status = server_error_code::ok;
}

template <>
auto
request_handler::intervals_get_levels<level_element_t>(
  const request &req, const query_container &query, response_header &resp_hdr,
  response_payload &resp_data) -> void {
  auto &lgr = logger::instance();
  std::error_code ec;
  std::vector<level_container<level_element_t>> levels;
  for (const auto &methylome_name : req.methylome_names) {
    const auto meth = methylomes.get_methylome(methylome_name, ec);
    if (ec) {
      lgr.error("Failed to load methylome {}: {}", methylome_name, ec);
      resp_hdr.status = ec;
      return;
    }
    lgr.debug("Computing levels for methylome: {}", methylome_name);
    levels.emplace_back(meth->get_levels<level_element_t>(query));
  }
  resp_data = response_payload::from_levels(levels, ec);
  if (ec) {
    lgr.debug("Error composing response payload: {}", ec);
    resp_hdr.status = server_error_code::bad_request;
  }
}

template <>
auto
request_handler::intervals_get_levels<level_element_covered_t>(
  const request &req, const query_container &query, response_header &resp_hdr,
  response_payload &resp_data) -> void {
  auto &lgr = logger::instance();
  std::error_code ec;
  std::vector<level_container<level_element_covered_t>> levels;
  for (const auto &methylome_name : req.methylome_names) {
    const auto meth = methylomes.get_methylome(methylome_name, ec);
    if (ec) {
      lgr.error("Failed to load methylome {}: {}", methylome_name, ec);
      resp_hdr.status = ec;
      return;
    }
    lgr.debug("Computing levels for methylome: {}", methylome_name);
    levels.emplace_back(meth->get_levels<level_element_covered_t>(query));
  }
  resp_data = response_payload::from_levels(levels, ec);
  if (ec) {
    lgr.debug("Error composing response payload: {}", ec);
    resp_hdr.status = server_error_code::bad_request;
  }
}

template <>
auto
request_handler::bins_get_levels<level_element_t>(
  const request &req, response_header &resp_hdr,
  response_payload &resp_data) -> void {
  auto &lgr = logger::instance();
  std::error_code ec;
  std::shared_ptr<genome_index> index = nullptr;
  std::string genome_name;
  std::vector<level_container<level_element_t>> levels;

  for (const auto &methylome_name : req.methylome_names) {
    const auto meth = methylomes.get_methylome(methylome_name, ec);
    if (ec) {
      lgr.error("Failed to load methylome {}: {}", methylome_name, ec);
      resp_hdr.status = ec;
      return;
    }
    // need genome index to know the bins
    if (index == nullptr) {
      genome_name = meth->get_genome_name();
      index = indexes.get_genome_index(genome_name, ec);
      if (ec) {
        lgr.error("Failed to load genome index for {}: {}", genome_name, ec);
        resp_hdr.status = ec;
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
    lgr.debug("Computing levels for methylome: {}", methylome_name);
    levels.emplace_back(
      meth->get_levels<level_element_t>(req.bin_size(), *index));
  }

  // convert the levels to data for sending
  resp_data = response_payload::from_levels(levels, ec);
  if (ec) {
    lgr.debug("Error composing response payload: {}", ec);
    resp_hdr.status = server_error_code::bad_request;
  }
}

template <>
auto
request_handler::bins_get_levels<level_element_covered_t>(
  const request &req, response_header &resp_hdr,
  response_payload &resp_data) -> void {
  auto &lgr = logger::instance();
  std::error_code ec;
  std::shared_ptr<genome_index> index = nullptr;
  std::string genome_name;
  std::vector<level_container<level_element_covered_t>> levels;

  for (const auto &methylome_name : req.methylome_names) {
    const auto meth = methylomes.get_methylome(methylome_name, ec);
    if (ec) {
      lgr.error("Failed to load methylome {}: {}", methylome_name, ec);
      resp_hdr.status = ec;
      return;
    }
    // need genome index to know the bins
    if (index == nullptr) {
      genome_name = meth->get_genome_name();
      index = indexes.get_genome_index(genome_name, ec);
      if (ec) {
        lgr.error("Failed to load genome index for {}: {}", genome_name, ec);
        resp_hdr.status = ec;
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
    lgr.debug("Computing levels for methylome: {}", methylome_name);
    levels.emplace_back(
      meth->get_levels<level_element_covered_t>(req.bin_size(), *index));
  }

  // convert the levels to data for sending
  resp_data = response_payload::from_levels(levels, ec);
  if (ec) {
    lgr.debug("Error composing response payload: {}", ec);
    resp_hdr.status = server_error_code::bad_request;
  }
}

}  // namespace transferase
