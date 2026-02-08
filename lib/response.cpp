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

#include "server_error_code.hpp"

#include <config.h>

#include <algorithm>
#include <cassert>
#include <charconv>  // for std::from_chars
#include <format>
#include <ranges>  // IWYU pragma: keep
#include <string>
#include <type_traits>  // for std::underlying_type_t

namespace transferase {

[[nodiscard]] auto
compose(char *first, const char *last,
        const response_header &hdr) noexcept -> std::error_code {
  // ADS: use to_chars here
  const auto s = std::format("{}\t{}\t{}\t{}\t{}\n", hdr.status.value(),
                             VERSION, hdr.cols, hdr.rows, hdr.n_bytes);
  assert(std::ranges::ssize(s) <
         std::distance(const_cast<const char *>(first), last));
  const auto data_end = std::ranges::copy(s, first);  // in_out_result
  if (data_end.out == last)
    return std::make_error_code(std::errc::result_out_of_range);
  return std::error_code{};
}

[[nodiscard]] auto
parse(const char *first, const char *last,
      response_header &hdr) noexcept -> std::error_code {
  static constexpr auto delim = '\t';
  static constexpr auto term = '\n';

  auto cursor = first;

  // status
  std::underlying_type_t<server_error_code> tmp{};
  {
    const auto [ptr, ec] = std::from_chars(cursor, last, tmp);
    if (ec != std::errc{})
      return server_error_code::server_failure;
    cursor = ptr;
  }
  hdr.status = static_cast<server_error_code>(tmp);
  if (*cursor != delim)
    return server_error_code::server_failure;
  ++cursor;

  // version
  const auto ver_end = std::find(cursor, last, delim);
  if (*ver_end != delim)
    return server_error_code::server_failure;
  hdr.xfr_version = std::string{cursor, ver_end};
  cursor = ver_end;
  ++cursor;

  // response cols
  {
    const auto [ptr, ec] = std::from_chars(cursor, last, hdr.cols);
    if (ec != std::errc{})
      return server_error_code::server_failure;
    cursor = ptr;
  }
  if (*cursor != delim)
    return server_error_code::server_failure;
  ++cursor;

  // response rows
  {
    const auto [ptr, ec] = std::from_chars(cursor, last, hdr.rows);
    if (ec != std::errc{})
      return server_error_code::server_failure;
    cursor = ptr;
  }
  ++cursor;

  // response n_bytes
  {
    const auto [ptr, ec] = std::from_chars(cursor, last, hdr.n_bytes);
    if (ec != std::errc{})
      return server_error_code::server_failure;
    cursor = ptr;
  }

  // terminator
  if (*cursor != term)
    return server_error_code::server_failure;
  ++cursor;

  return server_error_code::ok;
}

[[nodiscard]] auto
compose(
  response_header_buffer &buf,  // cppcheck-suppress constParameterReference
  const response_header &hdr) noexcept -> std::error_code {
  return compose(std::data(buf), std::data(buf) + resp_hdr_sz, hdr);
}

[[nodiscard]] auto
parse(const response_header_buffer &buf,
      response_header &hdr) noexcept -> std::error_code {
  return parse(std::data(buf), std::data(buf) + resp_hdr_sz, hdr);
}

[[nodiscard]] auto
response_header::summary() const noexcept -> std::string {
  static constexpr auto fmt =
    R"({{"{}": "{}", "VERSION": "{}", "cols": {}, "rows": {}, "n_bytes": {}}})";
  return std::format(fmt, status.category().name(), status.message(),
                     xfr_version, cols, rows, n_bytes);
}

}  // namespace transferase
