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
#include "methylome.hpp"
#include "request.hpp"
#include "response.hpp"
#include "utilities.hpp"

#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <print>
#include <regex>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using std::format;
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
request_handler::handle_header(const request &req, response &resp) -> void {
  // ADS: just to be sure?
  resp.methylome_size = 0;
  resp.counts.clear();

  // verify that the accession makes sense
  if (!is_valid_accession(req.accession)) {
    if (verbose)
      println("Malformed accession: {}.", req.accession);
    resp.status = status_code::invalid_accession;
    return;
  }

  // ADS TODO: move more of this into methylome_set?

  const auto get_methylome_start{hr_clock::now()};
  const auto meth_ec = ms.get_methylome(req.accession);
  const auto get_methylome_stop{hr_clock::now()};
  if (verbose)
    println("Elapsed time for get methylome: {:.3}s",
            duration(get_methylome_start, get_methylome_stop));
  if (get<1>(meth_ec)) {
    if (verbose)
      println("Methylome not found: {}", req.accession);
    resp.status = status_code::methylome_not_found;
    return;
  }

  // confirm that the methylome size is as expected
  if (req.methylome_size != ms.n_total_cpgs) {
    if (verbose)
      println("Incorrect methylome size (provided={}, expected={}).",
              req.methylome_size, ms.n_total_cpgs);
    resp.status = status_code::invalid_methylome_size;
    return;
  }

  // ADS TODO: set up the methylome for loading here?
  /* This would involve the methylome_set */

  // ADS TODO: allocate the space to start reading offsets?
  resp.methylome_size = req.methylome_size;
}

auto
request_handler::handle_get_counts(const request &req, response &resp) -> void {
  // assume methylome availability has been determined
  const auto [m, ec] = ms.get_methylome(req.accession);
  if (ec) {
    if (verbose)
      println("Failed to load methylome: {}", ec);
    resp.status = status_code::server_failure;
    return;
  }

  if (verbose)
    println("Computing counts for methylome: {}", req.accession);

  // generate the counts
  resp.counts = m->get_counts(req.offsets);
}
