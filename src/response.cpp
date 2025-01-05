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
#include "xfrase_error.hpp"

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
        const response_header &hdr) -> std::error_code {
  // ADS: use to_chars here
  const auto s = std::format("{}\t{}\n", hdr.status.value(), hdr.response_size);
  assert(std::ranges::ssize(s) <
         std::distance(const_cast<const char *>(first), last));
  const auto data_end = std::ranges::copy(s, first);  // in_out_result
  if (data_end.out == last)
    return std::make_error_code(std::errc::result_out_of_range);
  return std::error_code{};
}

[[nodiscard]] auto
parse(const char *first, const char *last,
      response_header &hdr) -> std::error_code {
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

  // response size
  {
    const auto [ptr, ec] = std::from_chars(cursor, last, hdr.response_size);
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
compose(response_header_buffer &buf,
        const response_header &hdr) -> std::error_code {
  return compose(buf.data(), buf.data() + response_header_buffer_size, hdr);
}

[[nodiscard]] auto
parse(const response_header_buffer &buf,
      response_header &hdr) -> std::error_code {
  return parse(buf.data(), buf.data() + response_header_buffer_size, hdr);
}

[[nodiscard]] auto
response_header::summary() const -> std::string {
  static constexpr auto fmt = R"({{"{}": "{}", "response_size": {}}})";
  return std::format(fmt, status.category().name(), status, response_size);
}

}  // namespace transferase
