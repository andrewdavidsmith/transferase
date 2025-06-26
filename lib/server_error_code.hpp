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

#ifndef LIB_SERVER_ERROR_CODE_HPP_
#define LIB_SERVER_ERROR_CODE_HPP_

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

enum class server_error_code : std::uint8_t {
  // clang-format off
  ok =                     0,
  invalid_methylome_name = 1,
  invalid_request_type =   2,
  too_many_intervals =     3,
  bin_size_too_small =     4,
  window_size_too_small =  5,
  window_step_too_small =  6,
  invalid_index_hash =     7,
  methylome_not_found =    8,
  index_not_found =        9,
  server_failure =        10,
  bad_request =           11,
  inconsistent_genomes =  12,
  connection_timeout   =  13,
  // clang-format on
};

template <>
struct std::is_error_code_enum<server_error_code> : public std::true_type {};

struct server_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override {return "server_error_code";}
  auto message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "invalid methylome name"s;
    case 2: return "invalid request type"s;
    case 3: return "too many intervals"s;
    case 4: return "bin size too small"s;
    case 5: return "window size too small"s;
    case 6: return "window step too small"s;
    case 7: return "invalid index hash"s;
    case 8: return "methylome not found"s;
    case 9: return "index not found"s;
    case 10: return "server failure"s;
    case 11: return "bad request"s;
    case 12: return "inconsistent genomes"s;
    case 13: return "connection timeout"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(server_error_code e) -> std::error_code {
  static auto category = server_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // LIB_SERVER_ERROR_CODE_HPP_
