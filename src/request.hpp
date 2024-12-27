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

#ifndef SRC_REQUEST_HPP_
#define SRC_REQUEST_HPP_

#include "utilities.hpp"     // for compose_result, parse_result
#include "xfrase_error.hpp"  // IWYU pragma: keep

#include <array>
#include <cstdint>  // for uint32_t
#include <format>
#include <iterator>  // for size
#include <string>
#include <system_error>
#include <type_traits>  // for true_type
#include <utility>      // for to_underlying, pair, unreachable
#include <vector>

enum class request_error : std::uint32_t {
  ok = 0,
  header_parse_error_accession = 1,
  header_parse_error_methylome_size = 2,
  header_parse_error_request_type = 3,
  lookup_parse_error_n_intervals = 4,
  lookup_error_reading_offsets = 5,
  lookup_error_offsets = 6,
  bins_error_assembly = 7,
  bins_error_bin_size = 8,
  noref_error_chroms_n_bytes = 9,
  noref_error_n_intervals = 10,
};

// register request_error as error code enum
template <>
struct std::is_error_code_enum<request_error> : public std::true_type {};

// category to provide text descriptions
struct request_error_category : std::error_category {
  auto
  name() const noexcept -> const char * override {
    return "request_error";
  }
  auto
  message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    // clang-format off
    switch (code) {
    case 0: return "ok"s;
    case 1: return "header parse error accession"s;
    case 2: return "header parse error methylome size"s;
    case 3: return "header parse error request type"s;
    case 4: return "lookup parse error n_intervals"s;
    case 5: return "lookup error reading offsets"s;
    case 6: return "lookup error offsets"s;
    case 7: return "bins error assembly"s;
    case 8: return "bins error bin size"s;
    case 9: return "noref error chroms_n_bytes"s;
    case 10: return "noref error n_intervals"s;
    }
    // clang-format on
    std::unreachable();  // hopefully this is unreacheable
  }
};

inline auto
make_error_code(request_error e) -> std::error_code {
  static auto category = request_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

static constexpr std::uint32_t request_header_buf_size = 256;  // full request
typedef std::array<char, request_header_buf_size> request_header_buffer;

struct request_header {
  enum class request_type : std::uint32_t {
    counts = 0,
    counts_cov = 1,
    bin_counts = 2,
    bin_counts_cov = 3,
    counts_noref = 4,
    counts_noref_cov = 5,
    n_request_types = 6,
  };
  std::string accession;
  std::uint32_t methylome_size{};
  request_type rq_type{};

  auto
  operator<=>(const request_header &) const = default;

  [[nodiscard]] auto
  summary() const -> std::string;

  [[nodiscard]] auto
  is_valid_type() const -> bool {
    return rq_type < request_type::n_request_types;
  }

  [[nodiscard]] auto
  is_intervals_request() const -> bool {
    return rq_type == request_type::counts ||
           rq_type == request_type::counts_cov;
  }

  [[nodiscard]] auto
  is_bins_request() const -> bool {
    return rq_type == request_type::bin_counts ||
           rq_type == request_type::bin_counts_cov;
  }

  [[nodiscard]] auto
  is_intervals_noref_request() const -> bool {
    return rq_type == request_type::counts_noref ||
           rq_type == request_type::counts_noref_cov;
  }
};

template <>
struct std::formatter<request_header::request_type>
  : std::formatter<std::string> {
  auto
  format(const request_header::request_type &rt,
         std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}", std::to_underlying(rt));
  }
};

template <>
struct std::formatter<request_header> : std::formatter<std::string> {
  auto
  format(const request_header &rh, std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}\t{}\t{}", rh.accession,
                          rh.methylome_size, rh.rq_type);
  }
};

[[nodiscard]] auto
compose(char *first, char *last,
        const request_header &header) -> compose_result;

[[nodiscard]] auto
parse(const char *first, const char *last,
      request_header &header) -> parse_result;

[[nodiscard]] auto
compose(request_header_buffer &buf,
        const request_header &header) -> compose_result;

[[nodiscard]] auto
parse(const request_header_buffer &buf, request_header &header) -> parse_result;

struct request {
  typedef std::pair<std::uint32_t, std::uint32_t> offset_type;
  std::uint32_t n_intervals{};
  std::vector<offset_type> offsets;

  [[nodiscard]] auto
  summary() const -> std::string;

  [[nodiscard]] auto
  get_query_n_bytes() const -> std::uint32_t {
    return sizeof(decltype(offsets)::value_type) * size(offsets);
  }
  [[nodiscard]] auto
  get_query_data() -> char * {
    return reinterpret_cast<char *>(offsets.data());
  }
  auto
  operator<=>(const request &) const = default;
};

// returns an compose_result which is a pair
[[nodiscard]] auto
compose(char *first, char *last, const request &req) -> compose_result;

[[nodiscard]] auto
parse(const char *first, const char *last, request &req) -> parse_result;

struct bins_request {
  std::uint32_t bin_size{};
  [[nodiscard]] auto
  summary() const -> std::string;
  auto
  operator<=>(const bins_request &) const = default;
};

[[nodiscard]] auto
compose(char *first, char *last, const bins_request &req) -> compose_result;

[[nodiscard]] auto
parse(const char *first, const char *last, bins_request &req) -> parse_result;

#endif  // SRC_REQUEST_HPP_
