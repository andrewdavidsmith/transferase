/* MIT License
 *
 * Copyright (c) 2025 Andrew D Smith
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

#ifndef LIB_HTTP_ERROR_CODE_HPP_
#define LIB_HTTP_ERROR_CODE_HPP_

#include <cstdint>
#include <string>
#include <system_error>
#include <utility>

/// @brief Enum for error codes related to http
enum class http_error_code : std::uint8_t {
  ok = 0,
  connect_failed = 1,
  handshake_failed = 2,
  send_request_failed = 3,
  receive_header_failed = 4,
  unknown_body_length = 5,
  reading_body_failed = 6,
  inactive_timeout = 7,
};

template <>
struct std::is_error_code_enum<http_error_code> : public std::true_type {};

struct http_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override {return "http";}
  auto message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "connect failed"s;
    case 2: return "handshake failed"s;
    case 3: return "send request failed"s;
    case 4: return "receive header failed"s;
    case 5: return "unknown body length"s;
    case 6: return "reading body failed"s;
    case 7: return "inactive timeout"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(http_error_code e) -> std::error_code {
  static auto category = http_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // LIB_HTTP_ERROR_CODE_HPP_
