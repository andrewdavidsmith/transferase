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

#ifndef SRC_STATUS_CODE_HPP_
#define SRC_STATUS_CODE_HPP_

#include <bitset>
#include <cstdint>
#include <format>
#include <string>
#include <type_traits>

namespace status_code {
enum value : std::uint32_t {
  ok = 0,
  indeterminate = 1 << 0,
  // parsing request
  malformed_accession = 1 << 1,
  malformed_methylome_size = 1 << 2,
  malformed_n_intervals = 1 << 3,
  malformed_offsets = 1 << 4,
  // handling request
  invalid_accession = 1 << 5,
  invalid_methylome_size = 1 << 6,
  // server-side problems
  index_not_found = 1 << 7,
  methylome_not_found = 1 << 8,
  // general server problem
  server_failure = 1 << 9,
  // others
  bad_request = 1 << 10,
};
static constexpr std::uint32_t n_codes = 12;

[[maybe_unused]] static const char *msg[] = {
  "ok",
  "indeterminate",
  // parsing request
  "malformed_accession",
  "malformed_methylome_size",
  "malformed_n_intervals",
  "malformed_offsets",
  // handling request
  "invalid_accession",
  "invalid_methylome_size",
  // server-side problems
  "index_not_found",
  "methylome_not_found",
  // general server problem
  "server_failure",
  // others
  "bad_request",
};

}  // end namespace status_code

template <>
struct std::formatter<status_code::value> : std::formatter<std::string> {
  auto format(const status_code::value &v, std::format_context &ctx) const {
    const auto u = std::bitset<status_code::n_codes>(static_cast<uint32_t>(v));
    return std::formatter<std::string>::format(u.to_string(), ctx);
  }
};

#endif  // SRC_STATUS_CODE_HPP_
