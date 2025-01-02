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

#include "cpg_index.hpp"
#include "cpg_index_metadata.hpp"
#include "logger.hpp"
#include "methylome.hpp"
#include "methylome_data.hpp"
#include "methylome_metadata.hpp"
#include "methylome_set.hpp"  // for is_valid_accession
#include "query.hpp"          // IWYU pragma: keep
#include "request.hpp"
#include "request_type_code.hpp"
#include "response.hpp"
#include "utilities.hpp"
#include "xfrase_error.hpp"

#include <chrono>       // for std::chrono::high_resolution_clock
#include <cstring>      // for std::memcpy
#include <iterator>     // for std::size, std::pair
#include <memory>       // for std::shared_ptr
#include <type_traits>  // for std::remove_cvref_t
#include <vector>

namespace xfrase {

auto
request_handler::add_response_size(const request &req,
                                   response_header &resp_hdr) -> void {
  // ADS: error codes in here should be converted to codes that will
  // make more sense on the other end of a connection: "can't read a
  // methylome file" => "can't find the methylome". Not the best
  // approach.
  auto &lgr = logger::instance();
  std::error_code ec;

  // assume methylome availability has been determined
  const auto meth = ms.get_methylome(req.accession, ec);
  if (ec) {
    lgr.warning("Error loading methylome {}: {}", req.accession, ec);
    resp_hdr.status = server_response_code::methylome_not_found;
    return;
  }

  const auto index = indexes.get_cpg_index(meth->meta.assembly, ec);
  if (ec) {
    lgr.error("Failed to load cpg index for {}: {}", meth->meta.assembly, ec);
    resp_hdr.status = server_response_code::index_not_found;
    return;
  }
  resp_hdr.response_size = req.is_intervals_request()
                             ? req.n_intervals()
                             : index->meta.get_n_bins(req.bin_size());
  resp_hdr.status = server_response_code::ok;
}

auto
request_handler::handle_request(const request &req,
                                response_header &resp_hdr) -> void {
  auto &lgr = logger::instance();
  resp_hdr.response_size = 0;

  // ADS: might be redundant here
  // verify that the accession makes sense
  if (!methylome::is_valid_name(req.accession)) {
    lgr.warning("Malformed accession: {}", req.accession);
    resp_hdr.status = server_response_code::invalid_accession;
    return;
  }

  // verify that the request type makes sense
  if (!req.is_valid_type()) {
    lgr.warning("Request type not valid: {}", req.request_type);
    resp_hdr.status = server_response_code::invalid_request_type;
    return;
  }

  std::error_code ec;
  const auto start_time{std::chrono::high_resolution_clock::now()};
  const auto meth = ms.get_methylome(req.accession, ec);
  const auto stop_time{std::chrono::high_resolution_clock::now()};
  lgr.debug("Elapsed time for get_methylome: {:.3}s",
            duration(start_time, stop_time));

  if (ec) {
    lgr.warning("Error loading methylome {}: {}", req.accession, ec);
    resp_hdr.status = server_response_code::methylome_not_found;
    return;
  }

  // confirm that the methylome size is as expected
  if (req.index_hash != meth->meta.index_hash) {
    lgr.warning("Incorrect index_hash (provided={}, expected={})",
                req.index_hash, meth->meta.index_hash);
    resp_hdr.status = server_response_code::invalid_index_hash;
    return;
  }

  // ADS: check for errors related to bins here also for early exit if an
  // error is found

  // ADS TODO: set up the methylome for loading here?
  /* This would involve the methylome_set */

  // ADS TODO: allocate the space to start reading offsets? Can't do
  // that if the number of intervals is not yet known
}

[[nodiscard]] static inline auto
levels_to_payload(auto &&levels) -> response_payload {
  // ADS: copy happening here is not needed
  using levels_res_type =
    typename std::remove_cvref_t<decltype(levels)>::value_type;
  const auto levels_n_bytes = sizeof(levels_res_type) * size(levels);
  response_payload r;
  r.payload.resize(levels_n_bytes);
  std::memcpy(r.payload.data(), levels.data(), levels_n_bytes);
  return r;
}

auto
request_handler::handle_get_levels(const request &req, const xfrase::query &qry,
                                   response_header &resp_hdr,
                                   response_payload &resp_data) -> void {
  auto &lgr = logger::instance();

  // assume methylome availability has been determined
  std::error_code ec;
  const auto meth = ms.get_methylome(req.accession, ec);
  if (ec) {
    lgr.error("Failed to load methylome {}: {}", req.accession, ec);
    resp_hdr.status = ec;
    return;
  }

  lgr.debug("Computing levels for methylome: {}", req.accession);

  if (req.request_type == request_type_code::intervals) {
    resp_data = levels_to_payload(meth->data.get_levels(qry));
    return;
  }
  if (req.request_type == request_type_code::intervals_covered) {
    resp_data = levels_to_payload(meth->data.get_levels_covered(qry));
    return;
  }

  // ADS: if we arrive here, the request was bad
  resp_hdr.status = server_response_code::bad_request;
}

auto
request_handler::handle_get_levels(const request &req,
                                   response_header &resp_hdr,
                                   response_payload &resp_data) -> void {
  auto &lgr = logger::instance();

  // assume methylome availability has been determined
  std::error_code ec;
  const auto meth = ms.get_methylome(req.accession, ec);
  if (ec) {
    lgr.error("Failed to load methylome {}: {}", req.accession, ec);
    resp_hdr.status = ec;
    return;
  }

  lgr.debug("Computing bins for methylome: {}", req.accession);

  // need cpg index to know what is in each bin
  const auto index = indexes.get_cpg_index(meth->meta.assembly, ec);
  if (ec) {
    lgr.error("Failed to load cpg index for {}: {}", meth->meta.assembly, ec);
    resp_hdr.status = ec;
    return;
  }

  if (req.request_type == request_type_code::bins) {
    resp_data = levels_to_payload(meth->get_levels(req.bin_size(), *index));
    return;
  }

  if (req.request_type == request_type_code::bins_covered) {
    resp_data =
      levels_to_payload(meth->get_levels_covered(req.bin_size(), *index));
    return;
  }

  // ADS: if we arrive here, the request was bad
  resp_hdr.status = server_response_code::bad_request;
}

}  // namespace xfrase
