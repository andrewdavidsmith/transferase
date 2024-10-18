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

#include <cstdint>
#include <format>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

enum class request_error : std::uint32_t {
  ok = 0,
  header_parse_error_accession = 1,
  header_parse_error_methylome_size = 2,
  header_parse_error_request_type = 3,
  lookup_parse_error_n_intervals = 4,
  lookup_error_reading_offsets = 5,
  lookup_error_offsets = 6,
};

static constexpr std::uint32_t request_error_n = 7;

// register request_error as error code enum
template <>
struct std::is_error_code_enum<request_error> : public std::true_type {};

// category to provide text descriptions
struct request_error_category : std::error_category {
  const char *name() const noexcept override { return "request_error"; }
  std::string message(int code) const override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0:
      return "ok"s;
    case 1:
      return "header parse error accession"s;
    case 2:
      return "header parse error methylome size"s;
    case 3:
      return "header parse error request type"s;
    case 4:
      return "lookup parse error n_intervals"s;
    case 5:
      return "lookup error reading offsets"s;
    case 6:
      return "lookup error offsets"s;
    }
    std::unreachable();  // hopefully this is unreacheable
  }
};

inline std::error_code
make_error_code(request_error e) {
  static auto category = request_error_category{};
  return std::error_code(std::to_underlying(e), category);
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

// register server_response_code as error code enum
template <>
struct std::is_error_code_enum<server_response_code> : public std::true_type {};

// category to provide text descriptions
struct server_response_category : std::error_category {
  const char *name() const noexcept override { return "server_response"; }
  std::string message(int code) const override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0:
      return "ok"s;
    case 1:
      return "invalid accession"s;
    case 2:
      return "invalid request type"s;
    case 3:
      return "invalid methylome size"s;
    case 4:
      return "methylome not found"s;
    case 5:
      return "index not found"s;
    case 6:
      return "server failure"s;
    case 7:
      return "bad request"s;
    }
    std::unreachable();  // hopefully
  }
};

inline std::error_code
make_error_code(server_response_code e) {
  static auto category = server_response_category{};
  return std::error_code(std::to_underlying(e), category);
}

// methylome_set errors

enum class methylome_set_code : std::uint32_t {
  ok = 0,
  invalid_accession = 1,
  methylome_file_not_found = 2,
  error_updating_live_methylomes = 3,
  error_reading_methylome_file = 4,
  methylome_already_live = 5,
};

static constexpr std::uint32_t methylome_set_code_n = 6;

// register methylome_set_code as error code enum
template <>
struct std::is_error_code_enum<methylome_set_code> : public std::true_type {};

// category to provide text descriptions
struct methylome_set_category : std::error_category {
  const char *name() const noexcept override { return "methylome_set"; }
  std::string message(int code) const override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0:
      return "ok"s;
    case 1:
      return "invalid accession"s;
    case 2:
      return "methylome file not found"s;
    case 3:
      return "error updating live methylomes"s;
    case 4:
      return "error reading methylome file"s;
    case 5:
      return "methylome already live"s;
    }
    std::unreachable();  // hopefully
  }
};

inline std::error_code
make_error_code(methylome_set_code e) {
  static auto category = methylome_set_category{};
  return std::error_code(std::to_underlying(e), category);
}

// methylome errors

enum class methylome_code : std::uint32_t {
  ok = 0,
  error_reading_methylome_header = 1,
  invalid_methylome_header = 2,
  error_reading_methylome = 3,
  error_decompressing_methylome = 4,
  error_compressing_methylome = 5,
  error_writing_methylome_header = 6,
  error_writing_methylome = 7,
  incorrect_methylome_size = 8,
};

static constexpr std::uint32_t methylome_code_n = 9;

// register methylome_code as error code enum
template <>
struct std::is_error_code_enum<methylome_code> : public std::true_type {};

// category to provide text descriptions
struct methylome_category : std::error_category {
  const char *name() const noexcept override { return "methylome"; }
  std::string message(int code) const override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0:
      return "ok"s;
    case 1:
      return "error reading methylome header"s;
    case 2:
      return "invalid methylome header"s;
    case 3:
      return "error reading methylome"s;
    case 4:
      return "error decompressing methylome"s;
    case 5:
      return "error compressing methylome"s;
    case 6:
      return "error writing methylome header"s;
    case 7:
      return "error writing methylome"s;
    case 8:
      return "incorrect methylome size"s;
    }
    std::unreachable();  // hopefully
  }
};

inline std::error_code
make_error_code(methylome_code e) {
  static auto category = methylome_category{};
  return std::error_code(std::to_underlying(e), category);
}

/*
  print std::error_code messages
 */
template <>
struct std::formatter<std::error_code> : std::formatter<std::string> {
  auto format(const std::error_code &e, std::format_context &ctx) const {
    return std::formatter<std::string>::format(std::format("{}", e.message()),
                                               ctx);
  }
};

/*
  print boost::system::error_code messages
 */
template <>
struct std::formatter<boost::system::error_code> : std::formatter<std::string> {
  auto format(const boost::system::error_code &e,
              std::format_context &ctx) const {
    return std::formatter<std::string>::format(std::format("{}", e.message()),
                                               ctx);
  }
};

#endif  // SRC_MC16_ERROR_HPP_
