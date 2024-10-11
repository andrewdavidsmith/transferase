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

#include "mc16_error.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <ranges>
#include <string>

namespace rg = std::ranges;
using std::format;
using std::from_chars;
using std::string;

auto
<<<<<<< HEAD
request::to_buffer(request_buffer &buf) -> request_error {
=======
request_header::to_buffer(request_buffer &buf) -> std::to_chars_result {
  // ADS: use to_chars here
  const string s = format("{}\t{}\t{}\n", accession, methylome_size,
                          request_type);
  rg::fill_n(begin(buf.buf), buf.buf_size, 0);
  auto data_end = rg::copy(s, begin(buf.buf));  // in_out_result
  return {data_end.out, std::errc{}};
}

auto
request_header::from_buffer(const request_buffer &buf) -> std::from_chars_result {
  static constexpr auto delim = '\t';
  static constexpr auto term = '\n';

  accession.clear();
  methylome_size = 0;
  request_type = 0;

  const auto data_end = cbegin(buf.buf) + buf.buf_size;
  auto cursor = rg::find(cbegin(buf.buf), data_end, delim);
  if (cursor == data_end)
    return {cursor, status_code::malformed_accession};
  accession = string(cbegin(buf.buf), rg::distance(cbegin(buf.buf), cursor));

  // methylome size
  if (*cursor++ != delim)
    return status_code::malformed_methylome_size;
  {
    const auto [ptr, ec] = from_chars(cursor, data_end, methylome_size);
    if (ec != std::errc{})
      return status_code::malformed_methylome_size;
    cursor = ptr;
  }

  // request type
  if (*cursor++ != delim)
    return status_code::malformed_request_type;
  {
    const auto [ptr, ec] = from_chars(cursor, data_end, request_type);
    if (ec != std::errc{})
      return status_code::malformed_request_type;
    cursor = ptr;
  }

  if (*cursor++ != term)
    return status_code::malformed_request_type;
  return status_code::ok;
}

auto
request::summary() const -> string {
  return format("accession: {}\n"
                "methylome_size: {}\n"
                "request_type: {}",
                accession, methylome_size, request_type);
}

auto
request::summary_serial() const -> string {
  return format("{{\"accession\": \"{}\", "
                "\"methylome_size\": {}, "
                "\"request_type\": {}}}",
                accession, methylome_size, request_type);
}

auto
request::to_buffer(request_buffer &buf) -> status_code::value {
>>>>>>> 31a3a46 (src/request.cpp: made partial modifications to use the request_header nested inside request)
  // ADS: use to_chars here
  const string s = format("{}\t{}\t{}\t{}\n", accession, methylome_size,
                          request_type, n_intervals);
  rg::fill_n(begin(buf.buf), buf.buf_size, 0);
  rg::copy(s, begin(buf.buf));
  return request_error::ok;
}

auto
request::from_buffer(const request_buffer &buf) -> request_error {
  static constexpr auto delim = '\t';
  static constexpr auto term = '\n';

  const auto hdr_status = header.from_buffer(buf);  // do like from_chars
  if (hdr_status) return hdr_status;
  n_intervals = 0;

  auto start_pos = rg::find(buf.buf, '\t');
  start_pos = rg::find(start_pos, '\t');
  start_pos = rg::find(buf.buf, '\t');

  const auto data_end = cbegin(buf.buf) + buf.buf_size;
  auto cursor = rg::find(cbegin(buf.buf), data_end, delim);
  if (cursor == data_end)
    return request_error::header_parse_error_accession;
  accession = string(cbegin(buf.buf), rg::distance(cbegin(buf.buf), cursor));

  // methylome size
  if (*cursor++ != delim)
    return request_error::header_parse_error_methylome_size;
  {
    const auto [ptr, ec] = from_chars(cursor, data_end, methylome_size);
    if (ec != std::errc{})
      return request_error::header_parse_error_methylome_size;
    cursor = ptr;
  }

  // request type
  if (*cursor++ != delim)
    return request_error::header_parse_error_request_type;
  {
    const auto [ptr, ec] = from_chars(cursor, data_end, request_type);
    if (ec != std::errc{})
      return request_error::header_parse_error_request_type;
    cursor = ptr;
  }

  // number of intervals
  if (*cursor++ != delim)
    return request_error::lookup_parse_error_n_intervals;
  {
    const auto [ptr, ec] = from_chars(cursor, data_end, n_intervals);
    if (ec != std::errc{})
      return request_error::lookup_parse_error_n_intervals;
    cursor = ptr;
  }

  if (*cursor++ != term)
    return request_error::lookup_parse_error_n_intervals;
  return request_error::ok;
}

auto
request::summary() const -> string {
  return format("{}\n" header.summary(), n_intervals);
}

auto
request::summary_serial() const -> string {
  return format("{{\"request_header\": \"{}\", "
                "\"n_intervals\": {}}}",
                header.summary_serial(), n_intervals);
}
