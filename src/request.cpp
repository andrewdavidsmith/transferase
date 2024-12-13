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

#include "mxe_error.hpp"  // IWYU pragma: keep

#include <algorithm>
#include <cassert>
#include <charconv>  // std::from_chars
#include <format>
#include <ranges>  // IWYU pragma: keep
#include <string>

[[nodiscard]] auto
compose(char *first, [[maybe_unused]] char *last,
        const request_header &header) -> compose_result {
  // ADS: use to_chars here
  const std::string s = std::format("{}\n", header);
  assert(static_cast<std::iterator_traits<char *>::difference_type>(
           std::size(s)) < std::distance(first, last));
  // std::ranges::in_out_result::out
  return {std::ranges::copy(s, first).out, request_error::ok};
}

[[nodiscard]] auto
parse(const char *first, const char *last,
      request_header &header) -> parse_result {
  static constexpr auto delim = '\t';
  static constexpr auto term = '\n';

  header.methylome_size = 0;
  header.rq_type = static_cast<request_header::request_type>(0);

  // accession
  header.accession.clear();
  auto cursor = std::find(first, last, delim);

  if (cursor == last)
    return {cursor, request_error::header_parse_error_accession};
  header.accession = std::string(first, std::distance(first, cursor));

  // methylome size
  if (*cursor != delim)
    return {cursor, request_error::header_parse_error_methylome_size};
  ++cursor;
  {
    const auto [ptr, ec] = std::from_chars(cursor, last, header.methylome_size);
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
    const auto [ptr, ec] = std::from_chars(cursor, last, tmp);
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

[[nodiscard]] auto
compose(request_header_buffer &buf,
        const request_header &hdr) -> compose_result {
  return compose(buf.data(), buf.data() + std::size(buf), hdr);
}

[[nodiscard]] auto
parse(const request_header_buffer &buf, request_header &hdr) -> parse_result {
  return parse(buf.data(), buf.data() + std::size(buf), hdr);
}

[[nodiscard]] auto
request_header::summary() const -> std::string {
  return std::format(R"({{"accession": "{}", )"
                     R"("methylome_size": {}, )"
                     R"("request_type": {}}})",
                     accession, methylome_size, rq_type);
}

// request

[[nodiscard]] auto
request::summary() const -> std::string {
  return std::format(R"({{"n_intervals": {}}})", n_intervals);
}

[[nodiscard]] auto
compose(char *first, [[maybe_unused]] char *last,
        const request &req) -> compose_result {
  // ADS: use to_chars here
  const std::string s = std::format("{}\n", req.n_intervals);
  assert(static_cast<std::iterator_traits<char *>::difference_type>(
           std::size(s)) < std::distance(first, last));
  // std::ranges::in_out_result::out
  return {std::ranges::copy(s, first).out, request_error::ok};
}

[[nodiscard]] auto
parse(const char *first, const char *last, request &req) -> parse_result {
  [[maybe_unused]] static constexpr auto delim = '\t';
  static constexpr auto term = '\n';

  auto cursor = first;

  // number of intervals
  const auto [ptr, ec] = std::from_chars(cursor, last, req.n_intervals);
  if (ec != std::errc{})
    return {ptr, request_error::lookup_parse_error_n_intervals};
  cursor = ptr;

  // terminator
  if (*cursor != term)
    return {cursor, request_error::lookup_parse_error_n_intervals};
  ++cursor;

  return {cursor, request_error::ok};
}

// bins_request

[[nodiscard]] auto
bins_request::summary() const -> std::string {
  return std::format(R"({{"bin_size": {}}})", bin_size);
}

[[nodiscard]] auto
compose(char *first, [[maybe_unused]] char *last,
        const bins_request &req) -> compose_result {
  static constexpr auto term = '\n';
  // ADS: use to_chars here
  const std::string s = std::format("{}{}", req.bin_size, term);
  assert(static_cast<std::iterator_traits<char *>::difference_type>(
           std::size(s)) < std::distance(first, last));
  // std::ranges::in_out_result::out
  return {std::ranges::copy(s, first).out, request_error::ok};
}

[[nodiscard]] auto
parse(const char *first, const char *last, bins_request &req) -> parse_result {
  static constexpr auto term = '\n';

  auto cursor = first;
  const auto [ptr, ec] = std::from_chars(cursor, last, req.bin_size);
  if (ec != std::errc{})
    return {ptr, request_error::bins_error_bin_size};
  cursor = ptr;

  // check terminator
  if (*cursor != term)
    return {cursor, request_error::bins_error_bin_size};
  ++cursor;

  return {cursor, request_error::ok};
}
