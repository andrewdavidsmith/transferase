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
#include "utilities.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>  // for std::from_chars
#include <format>
#include <ranges>  // IWYU pragma: keep
#include <string>

[[nodiscard]] auto
compose(char *first, [[maybe_unused]] char *last,
        const response_header &hdr) -> compose_result {
  // ADS: use to_chars here
  const auto s = std::format("{}\t{}\n", hdr.status.value(), hdr.response_size);
  assert(std::ranges::ssize(s) < std::distance(first, last));
  const auto data_end = std::ranges::copy(s, first);  // in_out_result
  return {data_end.out, std::error_code{}};
}

[[nodiscard]] auto
parse(const char *first, const char *last,
      response_header &hdr) -> parse_result {
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
    const auto [ptr, ec] = std::from_chars(cursor, last, hdr.response_size);
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

[[nodiscard]] auto
compose(std::array<char, response_buf_size> &buf,
        const response_header &hdr) -> compose_result {
  return compose(buf.data(), buf.data() + response_buf_size, hdr);
}

[[nodiscard]] auto
parse(const std::array<char, response_buf_size> &buf,
      response_header &hdr) -> parse_result {
  return parse(buf.data(), buf.data() + response_buf_size, hdr);
}

[[nodiscard]] auto
response_header::summary() const -> std::string {
  static constexpr auto fmt = R"({{"{}": "{}", "response_size": {}}})";
  return std::format(fmt, status.category().name(), status, response_size);
}
