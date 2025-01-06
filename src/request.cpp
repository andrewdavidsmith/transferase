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

#include "request_impl.hpp"
#include "request_type_code.hpp"  // IWYU pragma: keep

#include <algorithm>
#include <cassert>
#include <charconv>  // std::from_chars
#include <format>
#include <iterator>  // for std::size, std::distance, std::iterator_traits
#include <ranges>    // IWYU pragma: keep
#include <string>

namespace transferase {

[[nodiscard]] STATIC INLINE auto
compose(char *first, char const *last, const request &req) -> std::error_code {
  // ADS: use to_chars here
  const std::string s = std::format("{}\n", req);

#ifndef NDEBUG
  typedef std::iterator_traits<char *>::difference_type diff_type;
  const auto first_const = static_cast<char const *>(first);
  const auto dt_size_s = static_cast<diff_type>(std::size(s));
  assert(dt_size_s < std::distance(first_const, last));
#endif

  // std::ranges::in_out_result
  const auto data_end = std::ranges::copy(s, first);
  if (data_end.out == last)
    return std::make_error_code(std::errc::result_out_of_range);
  return std::error_code{};
}

[[nodiscard]] STATIC inline auto
parse(char const *first, char const *last, request &req) -> std::error_code {
  static constexpr auto delim = '\t';
  static constexpr auto term = '\n';

  req.index_hash = 0;
  req.request_type = request_type_code::unknown;

  // accession
  req.accession.clear();
  auto cursor = std::find(first, last, delim);

  if (cursor == last)
    return request_error::parse_error_accession;
  req.accession = std::string(first, std::distance(first, cursor));

  // request type
  if (*cursor != delim)
    return request_error::parse_error_request_type;
  ++cursor;
  {
    std::underlying_type_t<request_type_code> tmp{};
    const auto [ptr, ec] = std::from_chars(cursor, last, tmp);
    if (ec != std::errc{})
      return request_error::parse_error_request_type;
    req.request_type = static_cast<request_type_code>(tmp);
    cursor = ptr;
  }

  // index hash
  if (*cursor != delim)
    return request_error::parse_error_index_hash;
  ++cursor;
  {
    const auto [ptr, ec] = std::from_chars(cursor, last, req.index_hash);
    if (ec != std::errc{})
      return request_error::parse_error_index_hash;
    cursor = ptr;
  }

  // aux value (either n_intervals or bin_size)
  if (*cursor != delim)
    return request_error::parse_error_aux_value;
  ++cursor;
  {
    const auto [ptr, ec] = std::from_chars(cursor, last, req.aux_value);
    if (ec != std::errc{})
      return request_error::parse_error_aux_value;
    cursor = ptr;
  }

  if (*cursor != term)
    return request_error::parse_error_aux_value;
  ++cursor;

  return std::error_code{};
}

[[nodiscard]] auto
compose(request_buffer &buf, const request &req) -> std::error_code {
  return compose(buf.data(), buf.data() + std::size(buf), req);
}

[[nodiscard]] auto
parse(const request_buffer &buf, request &req) -> std::error_code {
  return parse(buf.data(), buf.data() + std::size(buf), req);
}

[[nodiscard]] auto
request::summary() const -> std::string {
  return std::format(R"({{"accession": "{}", )"
                     R"("request_type": {}, )"
                     R"("index_hash": {}, )"
                     R"("aux_value": {}}})",
                     accession, request_type, index_hash, aux_value);
}

}  // namespace transferase
