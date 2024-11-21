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
#include "mxe_error.hpp"
#include "request.hpp"
#include "response.hpp"
#include "utilities.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <print>
#include <regex>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using std::get;
using std::pair;
using std::println;
using std::string;
using std::uint32_t;

using hr_clock = std::chrono::high_resolution_clock;

[[nodiscard]] static auto
is_valid_accession(const string &accession) -> bool {
  static constexpr auto experiment_ptrn = R"(^(D|E|S)RX\d+$)";
  std::regex experiment_re(experiment_ptrn);
  return std::regex_search(accession, experiment_re);
}

// ADS: This function needs a complete request *except* it does not
// need the offsets to have been allocated or have any values read
// into them.
auto
request_handler::handle_header(const request_header &req_hdr,
                               response_header &resp_hdr) -> void {
  resp_hdr.methylome_size = 0;

  logger &lgr = logger::instance();

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

  // ADS TODO: move more of this into methylome_set?

  const auto get_methylome_start{hr_clock::now()};

  std::error_code get_meth_err;
  std::tie(std::ignore, get_meth_err) = ms.get_methylome(req_hdr.accession);
  const auto get_methylome_stop{hr_clock::now()};
  lgr.debug("Elapsed time for get methylome: {:.3}s",
            duration(get_methylome_start, get_methylome_stop));
  if (get_meth_err) {
    lgr.warning("Error loading methylome: {}", req_hdr.accession);
    lgr.warning("Error: {}", get_meth_err);
    resp_hdr.status = server_response_code::methylome_not_found;
    return;
  }

  // confirm that the methylome size is as expected
  if (req_hdr.methylome_size != ms.n_total_cpgs) {
    lgr.warning("Incorrect methylome size (provided={}, expected={})",
                req_hdr.methylome_size, ms.n_total_cpgs);
    resp_hdr.status = server_response_code::invalid_methylome_size;
    return;
  }

  // ADS TODO: set up the methylome for loading here?
  /* This would involve the methylome_set */

  // ADS TODO: allocate the space to start reading offsets? Can't do
  // that if the number of intervals is not yet known
  resp_hdr.methylome_size = req_hdr.methylome_size;
}

static inline auto
counts_to_payload(const auto &counts, const request_header &req_hdr,
                  const request &req) -> response_payload {
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
  const auto [meth, get_meth_err] = ms.get_methylome(req_hdr.accession);
  if (get_meth_err) {
    lgr.error("Failed to load methylome: {}", get_meth_err);
    // keep methylome size in response header
    resp_hdr.status = server_response_code::server_failure;
    return;
  }

  lgr.debug("Computing counts for methylome: {}", req_hdr.accession);

  switch (req_hdr.rq_type) {
  case request_header::request_type::counts:
    resp_data = counts_to_payload(meth->get_counts(req.offsets), req_hdr, req);
    return;
  case request_header::request_type::counts_cov:
    resp_data =
      counts_to_payload(meth->get_counts_cov(req.offsets), req_hdr, req);
    return;
    // case request_header::request_type::bin_counts:
    //   resp_data =
    //     counts_to_payload(meth->get_bins(req.offsets, index), req_hdr, req);
    //   return;
  }
}
