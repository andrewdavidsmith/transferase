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

#include "response.hpp"
#include "mc16_error.hpp"
#include "utilities.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <ranges>
#include <string>

using std::array;
using std::format;
using std::string;

namespace rg = std::ranges;

auto
to_chars(char *first, [[maybe_unused]] char *last,
         const response_header &hdr) -> mc16_to_chars_result {
  // ADS: use to_chars here
  const string s = format("{}\t{}\n", hdr.status.value(), hdr.methylome_size);
  assert(ssize(s) < std::distance(first, last));
  auto data_end = rg::copy(s, first);  // in_out_result
  return {data_end.out, request_error::ok};
}

auto
from_chars(const char *first, const char *last,
           response_header &hdr) -> mc16_from_chars_result {
  static constexpr auto delim = '\t';
  static constexpr auto term = '\n';

  auto cursor = first;

  // status
  int v{};
  {
    const auto [ptr, ec] = std::from_chars(cursor, last, v);
    if (ec != std::errc{})
      return {ptr, server_response_code::server_failure};
    cursor = ptr;
  }
  hdr.status = make_error_code(static_cast<server_response_code>(v));

  if (*cursor != delim)
    return {cursor, server_response_code::server_failure};
  ++cursor;

  // methylome size
  {
    const auto [ptr, ec] = std::from_chars(cursor, last, hdr.methylome_size);
    if (ec != std::errc{})
      return {ptr, server_response_code::server_failure};
    cursor = ptr;
  }

  // terminator
  if (*cursor != term)
    return {cursor, server_response_code::server_failure};
  ++cursor;

  return {cursor, server_response_code::ok};
}

auto
compose(array<char, response_buf_size> &buf,
        const response_header &hdr) -> compose_result {
  return to_chars(buf.data(), buf.data() + response_buf_size, hdr);
}

auto
parse(const array<char, response_buf_size> &buf,
      response_header &hdr) -> parse_result {
  return from_chars(buf.data(), buf.data() + response_buf_size, hdr);
}

auto
response_header::not_found() -> response_header {
  return {server_response_code::methylome_not_found, 0};
}

auto
response_header::bad_request() -> response_header {
  return {server_response_code::bad_request, 0};
}

auto
response_header::summary() const -> string {
  static constexpr auto fmt = "{}: \"{}\"\nmethylome_size: {}";
  return std::format(fmt, status.category().name(), status.message(),
                     methylome_size);
}

auto
response_header::summary_serial() const -> string {
  static constexpr auto fmt = R"({{"{}": "{}", "methylome_size": {}}})";
  return std::format(fmt, status.category().name(), status.message(),
                     methylome_size);
}
