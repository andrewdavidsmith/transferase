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

#include "request.hpp"

#include "mxe_error.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <ranges>
#include <string>

namespace rg = std::ranges;
using std::format;
using std::from_chars;
using std::size;
using std::string;

auto
to_chars(char *first, [[maybe_unused]] char *last,
         const request_header &header) -> mxe_to_chars_result {
  // ADS: use to_chars here
  const string s = format("{}\n", header);
  assert(static_cast<std::iterator_traits<char *>::difference_type>(size(s)) <
         std::distance(first, last));
  auto data_end = rg::copy(s, first);  // rg::in_out_result
  return {data_end.out, request_error::ok};
}

auto
from_chars(const char *first, const char *last,
           request_header &header) -> mxe_from_chars_result {
  static constexpr auto delim = '\t';
  static constexpr auto term = '\n';

  header.methylome_size = 0;
  header.rq_type = static_cast<request_header::request_type>(0);

  // accession
  header.accession.clear();
  auto cursor = std::find(first, last, delim);

  if (cursor == last)
    return {cursor, request_error::header_parse_error_accession};
  header.accession = string(first, std::distance(first, cursor));

  // methylome size
  if (*cursor != delim)
    return {cursor, request_error::header_parse_error_methylome_size};
  ++cursor;
  {
    const auto [ptr, ec] = from_chars(cursor, last, header.methylome_size);
    if (ec != std::errc{})
      return {ptr, request_error::header_parse_error_methylome_size};
    cursor = ptr;
  }

  // request type
  if (*cursor != delim)
    return {cursor, request_error::header_parse_error_request_type};
  ++cursor;
  {
    std::underlying_type_t<request_header::request_type> tmp{};
    const auto [ptr, ec] = from_chars(cursor, last, tmp);
    if (ec != std::errc{})
      return {ptr, request_error::header_parse_error_request_type};
    header.rq_type = static_cast<request_header::request_type>(tmp);
    cursor = ptr;
  }

  if (*cursor != term)
    return {cursor, request_error::header_parse_error_request_type};
  ++cursor;

  return {cursor, request_error::ok};
}

auto
compose(request_buffer &buf, const request_header &hdr) -> compose_result {
  return to_chars(buf.data(), buf.data() + size(buf), hdr);
}

auto
parse(const request_buffer &buf, request_header &hdr) -> parse_result {
  return from_chars(buf.data(), buf.data() + size(buf), hdr);
}

auto
request_header::summary() const -> string {
  return format(R"({{"accession": "{}", )"
                R"("methylome_size": {}, )"
                R"("request_type": {}}})",
                accession, methylome_size, rq_type);
}

// request

auto
request::summary() const -> string {
  return format(R"({{"n_intervals": {}}})", n_intervals);
}

auto
to_chars(char *first, [[maybe_unused]] char *last,
         const request &req) -> mxe_to_chars_result {
  // ADS: use to_chars here
  const string s = format("{}\n", req.n_intervals);
  assert(static_cast<std::iterator_traits<char *>::difference_type>(size(s)) <
         std::distance(first, last));
  auto data_end = rg::copy(s, first);  // rg::in_out_result
  return {data_end.out, request_error::ok};
}

auto
from_chars(const char *first, const char *last,
           request &req) -> mxe_from_chars_result {
  [[maybe_unused]] static constexpr auto delim = '\t';
  static constexpr auto term = '\n';

  auto cursor = first;

  // number of intervals
  {
    const auto [ptr, ec] = from_chars(cursor, last, req.n_intervals);
    if (ec != std::errc{})
      return {ptr, request_error::lookup_parse_error_n_intervals};
    cursor = ptr;
  }

  // terminator
  if (*cursor != term)
    return {cursor, request_error::lookup_parse_error_n_intervals};
  ++cursor;

  return {cursor, request_error::ok};
}
