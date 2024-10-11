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

#ifndef SRC_MC16_ERROR_HPP_
#define SRC_MC16_ERROR_HPP_

#include <boost/system.hpp>

#include <format>
#include <cstdint>
#include <string>
#include <type_traits>
#include <system_error>
#include <utility>

enum class request_error : std::uint32_t {
  ok = 0,
  header_parse_error_accession = 1,
  header_parse_error_methylome_size = 2,
  header_parse_error_request_type = 3,
  lookup_parse_error_n_intervals = 4,
  lookup_error_offsets = 5,
};

static constexpr std::uint32_t request_error_n = 6;

// register request_error as error condition enum
template <>
struct std::is_error_condition_enum<request_error> : public std::true_type {};

// category to provide text descriptions
struct request_error_category : std::error_category {
  const char* name() const noexcept override {
    return "request_error";
  }
  std::string message(int condition) const override {
    using namespace std::string_literals;
    switch(condition) {
    case 0: return "ok"s;
    case 1: return "header parse error accession"s;
    case 2: return "header parse error methylome size"s;
    case 3: return "header parse error request type"s;
    case 4: return "lookup parse error n_intervals"s;
    case 5: return "lookup error offsets"s;
    }
    std::abort(); // unreacheable
  }
};

inline
std::error_condition make_error_condition(request_error e) {
  static auto category = request_error_category{};
  return std::error_condition(std::to_underlying(e), category);
}

enum class server_response_code : std::uint32_t {
  ok = 0,
  invalid_accession = 1,
  invalid_request_type = 2,
  invalid_methylome_size = 3,
  methylome_not_found = 4,
  index_not_found = 5,
  server_failure = 6,
  bad_request = 7,
};

static constexpr std::uint32_t server_response_code_n = 8;

// register request_header_parse_error as error code enum
template <> struct
std::is_error_condition_enum<server_response_code> : public std::true_type {};

// category to provide text descriptions
struct server_response_category : std::error_category {
  const char* name() const noexcept override {
    return "server_response";
  }
  std::string message(int condition) const override {
    using namespace std::string_literals;
    switch(condition) {
    case 0: return "ok"s;
    case 1: return "invalid accession"s;
    case 2: return "invalid request type"s;
    case 3: return "invalid methylome size"s;
    case 4: return "methylome not found"s;
    case 5: return "index not found"s;
    case 6: return "server failure"s;
    case 7: return "bad request"s;
    }
    std::abort(); // unreacheable
  }
};

inline
std::error_condition make_error_condition(server_response_code e) {
  static auto category = server_response_category{};
  return std::error_condition(std::to_underlying(e), category);
}

template <>
struct std::formatter<std::error_condition> : std::formatter<std::string> {
  auto format(const std::error_condition &e, std::format_context &ctx) const {
    return std::formatter<std::string>::format(
      std::format(R"({}: "{}")",
                  e.category().name(), e.message()), ctx);
  }
};

/*
  Helper to print messages from boost::system::error_code
 */
template <>
struct std::formatter<boost::system::error_code> : std::formatter<std::string> {
  auto format(const boost::system::error_code &e, std::format_context &ctx) const {
    return std::formatter<std::string>::format(
      std::format(R"({}: "{}")",
                  e.category().name(), e.message()), ctx);
  }
};

#endif  // SRC_MC16_ERROR_HPP_
