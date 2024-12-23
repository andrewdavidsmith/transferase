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

#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_data.hpp"
#include "methylome_metadata.hpp"
#include "methylome_results_types.hpp"  // IWYU pragma: keep
#include "methylome_set.hpp"            // for is_valid_accession
#include "request.hpp"
#include "response.hpp"
#include "utilities.hpp"
#include "xfrase_error.hpp"

#include <chrono>    // for std::chrono::high_resolution_clock
#include <cstdint>   // for std::uint32_t
#include <cstring>   // for std::memcpy
#include <iterator>  // for std::size, std::pair
#include <memory>    // for std::shared_ptr
#include <print>
#include <string>
#include <type_traits>  // for std::remove_cvref_t
#include <vector>

using std::println;
using std::string;
using std::uint32_t;

auto
request_handler::add_response_size_for_bins(const request_header &req_hdr,
                                            const bins_request &req,
                                            response_header &resp_hdr) -> void {
  auto &lgr = logger::instance();
  std::error_code ec;

  // assume methylome availability has been determined
  const auto meth = ms.get_methylome(req_hdr.accession, ec);
  if (ec) {
    lgr.error("Failed to load methylome: {}", ec);
    resp_hdr.status = server_response_code::server_failure;
    return;
  }

  const auto [cim, meta_err] =
    indexes.get_cpg_index_metadata(meth->meta.assembly);
  if (meta_err) {
    lgr.error("Failed to load cpg index metadata for {}: {}",
              meth->meta.assembly, meta_err);
    resp_hdr.status = server_response_code::index_not_found;
    return;
  }
  resp_hdr.response_size = cim.get_n_bins(req.bin_size);
  resp_hdr.status = server_response_code::ok;
}

auto
request_handler::add_response_size_for_intervals(
  [[maybe_unused]] const request_header &req_hdr, const request &req,
  response_header &resp_hdr) -> void {
  resp_hdr.response_size = req.n_intervals;
}

// ADS: This function needs a complete request *except* it does not
// need the offsets to have been allocated or have any values read
// into them.
auto
request_handler::handle_header(const request_header &req_hdr,
                               response_header &resp_hdr) -> void {
  auto &lgr = logger::instance();

  resp_hdr.response_size = 0;

  // verify that the accession makes sense
  if (!is_valid_accession(req_hdr.accession)) {
    lgr.warning("Malformed accession: {}", req_hdr.accession);
    resp_hdr.status = server_response_code::invalid_accession;
    return;
  }

  // verify that the request type makes sense
  if (!req_hdr.is_valid_type()) {
    lgr.warning("Request type not valid: {}", req_hdr.rq_type);
    resp_hdr.status = server_response_code::invalid_request_type;
    return;
  }

  std::error_code get_meth_err;
  const auto get_methylome_start{std::chrono::high_resolution_clock::now()};
  const auto meth = ms.get_methylome(req_hdr.accession, get_meth_err);
  const auto get_methylome_stop{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for get methylome: {:.3}s",
            duration(get_methylome_start, get_methylome_stop));

  if (get_meth_err) {
    lgr.warning("Error loading methylome: {}", req_hdr.accession);
    lgr.warning("Error: {}", get_meth_err);
    resp_hdr.status = server_response_code::methylome_not_found;
    return;
  }

  // confirm that the methylome size is as expected
  if (req_hdr.methylome_size != meth->meta.n_cpgs) {
    lgr.warning("Incorrect methylome size (provided={}, expected={})",
                req_hdr.methylome_size, meth->meta.n_cpgs);
    resp_hdr.status = server_response_code::invalid_methylome_size;
    return;
  }

  // ADS: check for errors related to bins here also for early exit if an
  // error is found

  // ADS TODO: set up the methylome for loading here?
  /* This would involve the methylome_set */

  // ADS TODO: allocate the space to start reading offsets? Can't do
  // that if the number of intervals is not yet known
  resp_hdr.response_size = req_hdr.methylome_size;
}

static inline auto
counts_to_payload(const auto &counts) -> response_payload {
  using counts_res_type =
    typename std::remove_cvref_t<decltype(counts)>::value_type;
  const auto counts_n_bytes = sizeof(counts_res_type) * size(counts);
  response_payload r;
  r.payload.resize(counts_n_bytes);
  std::memcpy(r.payload.data(), counts.data(), counts_n_bytes);
  return r;
}

auto
request_handler::handle_get_counts(const request_header &req_hdr,
                                   const request &req,
                                   response_header &resp_hdr,
                                   response_payload &resp_data) -> void {
  logger &lgr = logger::instance();

  // assume methylome availability has been determined
  std::error_code get_meth_err;
  const auto meth = ms.get_methylome(req_hdr.accession, get_meth_err);
  if (get_meth_err) {
    lgr.error("Failed to load methylome: {}", get_meth_err);
    // keep methylome size in response header
    resp_hdr.status = server_response_code::server_failure;
    return;
  }

  lgr.debug("Computing counts for methylome: {}", req_hdr.accession);

  if (req_hdr.rq_type == request_header::request_type::counts) {
    resp_data = counts_to_payload(meth->data.get_counts(req.offsets));
    return;
  }
  if (req_hdr.rq_type == request_header::request_type::counts_cov) {
    resp_data = counts_to_payload(meth->data.get_counts_cov(req.offsets));
    return;
  }

  // ADS: if we arrive here, the request was bad
  resp_hdr.status = server_response_code::bad_request;
}

auto
request_handler::handle_get_bins(const request_header &req_hdr,
                                 const bins_request &req,
                                 response_header &resp_hdr,
                                 response_payload &resp_data) -> void {
  auto &lgr = logger::instance();

  // assume methylome availability has been determined
  std::error_code ec;
  const auto meth = ms.get_methylome(req_hdr.accession, ec);
  if (ec) {
    lgr.error("Failed to load methylome: {}", ec);
    // keep methylome size in response header
    resp_hdr.status = server_response_code::server_failure;
    return;
  }

  lgr.debug("Computing bins for methylome: {}", req_hdr.accession);

  // need cpg index to know what is in each bin
  const auto [index, cim, index_err] =
    indexes.get_cpg_index_with_meta(meth->meta.assembly);
  if (index_err) {
    lgr.error("Failed to load cpg index for {}: {}", meth->meta.assembly,
              index_err);
    resp_hdr.status = server_response_code::index_not_found;
    return;
  }

  if (req_hdr.rq_type == request_header::request_type::bin_counts) {
    resp_data =
      counts_to_payload(meth->data.get_bins(req.bin_size, index, cim));
    return;
  }

  if (req_hdr.rq_type == request_header::request_type::bin_counts_cov) {
    resp_data =
      counts_to_payload(meth->data.get_bins_cov(req.bin_size, index, cim));
    return;
  }

  // ADS: if we arrive here, the request was bad
  resp_hdr.status = server_response_code::bad_request;
}
