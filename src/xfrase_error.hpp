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

#ifndef SRC_XFRASE_ERROR_HPP_
#define SRC_XFRASE_ERROR_HPP_

#include <boost/system.hpp>

#include <cstdint>
#include <format>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

enum class server_response_code : std::uint32_t {
  ok = 0,
  invalid_accession = 1,
  invalid_request_type = 2,
  invalid_index_hash = 3,
  methylome_not_found = 4,
  index_not_found = 5,
  server_failure = 6,
  bad_request = 7,
};

// register server_response_code as error code enum
template <>
struct std::is_error_code_enum<server_response_code> : public std::true_type {};

// category to provide text descriptions
struct server_response_category : std::error_category {
  auto
  name() const noexcept -> const char * override {
    return "server_response";
  }
  auto
  message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    // clang-format off
    switch (code) {
    case 0: return "ok"s;
    case 1: return "invalid accession"s;
    case 2: return "invalid request type"s;
    case 3: return "invalid index hash"s;
    case 4: return "methylome not found"s;
    case 5: return "index not found"s;
    case 6: return "server failure"s;
    case 7: return "bad request"s;
    }
    // clang-format on
    std::unreachable();  // hopefully
  }
};

inline auto
make_error_code(server_response_code e) -> std::error_code {
  static auto category = server_response_category{};
  return std::error_code(std::to_underlying(e), category);
}

// print std::error_code messages
template <>
struct std::formatter<std::error_code> : std::formatter<std::string> {
  auto
  format(const std::error_code &e, std::format_context &ctx) const {
    return std::formatter<std::string>::format(e.message(), ctx);
  }
};

// print boost::system::error_code messages
template <>
struct std::formatter<boost::system::error_code> : std::formatter<std::string> {
  auto
  format(const boost::system::error_code &e, std::format_context &ctx) const {
    return std::formatter<std::string>::format(e.message(), ctx);
  }
};

#endif  // SRC_XFRASE_ERROR_HPP_
