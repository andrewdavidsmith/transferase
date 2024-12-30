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

#include "request_type_code.hpp"

#include <array>
#include <cstdint>  // for uint32_t
#include <format>
#include <string>
#include <system_error>
#include <type_traits>  // for true_type
#include <utility>      // for to_underlying, pair, unreachable

static constexpr std::uint32_t request_buffer_size{256};
typedef std::array<char, request_buffer_size> request_buffer;

struct request {
  std::string accession;
  request_type_code request_type{};
  std::uint64_t index_hash{};
  std::uint64_t aux_value{};

  [[nodiscard]] auto
  n_intervals() const {
    return is_intervals_request() ? aux_value : 0;
  }

  [[nodiscard]] auto
  bin_size() const {
    return is_bins_request() ? aux_value : 0;
  }

  [[nodiscard]] auto
  operator<=>(const request &) const = default;

  [[nodiscard]] auto
  summary() const -> std::string;

  [[nodiscard]] auto
  is_valid_type() const -> bool {
    return request_type < request_type_code::n_request_types;
  }

  [[nodiscard]] auto
  is_intervals_request() const -> bool {
    return request_type == request_type_code::counts ||
           request_type == request_type_code::counts_cov;
  }

  [[nodiscard]] auto
  is_bins_request() const -> bool {
    return request_type == request_type_code::bin_counts ||
           request_type == request_type_code::bin_counts_cov;
  }
};

template <> struct std::formatter<request> : std::formatter<std::string> {
  auto
  format(const request &r, std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}\t{}\t{}", r.accession, r.index_hash,
                          r.request_type, r.aux_value);
  }
};

[[nodiscard]] auto
compose(char *first, char const *last, const request &req) -> std::error_code;

[[nodiscard]] auto
parse(char *const first, char *const last, request &req) -> std::error_code;

[[nodiscard]] auto
compose(request_buffer &buf, const request &req) -> std::error_code;

[[nodiscard]] auto
parse(const request_buffer &buf, request &req) -> std::error_code;

// request error code
enum class request_error : std::uint32_t {
  ok = 0,
  parse_error_accession = 1,
  parse_error_index_hash = 2,
  parse_error_request_type = 3,
  parse_error_aux_value = 4,
  error_reading_query = 5,
};

template <>
struct std::is_error_code_enum<request_error> : public std::true_type {};
struct request_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override {return "request_error";}
  auto message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "error parsing accession"s;
    case 2: return "error parsing index_hash"s;
    case 3: return "error parsing request_type"s;
    case 4: return "error parsing aux_value"s;
    case 5: return "error reading query"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(request_error e) -> std::error_code {
  static auto category = request_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_REQUEST_HPP_
