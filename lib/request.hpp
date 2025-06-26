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

#ifndef LIB_REQUEST_HPP_
#define LIB_REQUEST_HPP_

#include "request_type_code.hpp"

#include <array>
#include <cstdint>  // for uint32_t
#include <format>
#include <iterator>  // for std::size
#include <string>
#include <system_error>
#include <type_traits>  // for true_type
#include <utility>      // for to_underlying, pair, unreachable
#include <vector>

namespace transferase {

static constexpr std::uint32_t request_buffer_size{512};
typedef std::array<char, request_buffer_size> request_buffer;

struct request {
  static constexpr auto max_methylomes_per_request = 50;
  static constexpr auto max_intervals_default = 2'000'000;
  static constexpr auto min_bin_size_default = 100;
  static constexpr auto min_window_size_default = 100;
  static constexpr auto min_window_step_default = 50;

  static std::uint32_t max_intervals;
  static std::uint32_t min_bin_size;
  static std::uint32_t min_window_size;
  static std::uint32_t min_window_step;

  request_type_code request_type{};
  std::uint64_t index_hash{};
  std::uint64_t aux_value{};
  std::vector<std::string> methylome_names;

  [[nodiscard]] auto
  n_methylomes() const {
    return std::size(methylome_names);
  }

  [[nodiscard]] auto
  n_intervals() const {
    return is_intervals_request() ? aux_value : 0;
  }

  [[nodiscard]] auto
  bin_size() const {
    return is_bins_request() ? aux_value : 0;
  }

  [[nodiscard]] auto
  window_size() const {
    return is_windows_request() ? (aux_value >> 32) : 0;
  }

  [[nodiscard]] auto
  window_step() const {
    return is_windows_request() ? (aux_value & 0xffffffff) : 0;
  }

  [[nodiscard]] static auto
  get_aux_for_windows(const std::uint64_t sz,
                      const std::uint64_t stp) -> std::uint64_t {
    return sz << 32 | stp;
  };

  [[nodiscard]] auto
  operator<=>(const request &) const = default;

  [[nodiscard]] auto
  summary() const -> std::string;

  [[nodiscard]] auto
  is_valid_aux_value() const -> bool {
    if (is_intervals_request())
      return aux_value < max_intervals;
    if (is_bins_request())
      return aux_value >= min_bin_size;
    if (is_windows_request()) {
      return window_size() >= min_window_size &&
             window_step() >= min_window_step;
    }
    std::unreachable{};
  }

  [[nodiscard]] auto
  get_invalid_aux_error_code() const -> std::error_code {
    if (is_intervals_request())
      return server_error_code::too_many_intervals;
    if (is_bins_request())
      return server_error_code::bin_size_too_small;
    if (is_windows_request()) {
      return window_size() < min_window_size
        ? server_error_code::window_size_too_small
        ? server_error_code::window_step_too_small;
    }
    std::unreachable{};
  }

  [[nodiscard]] auto
  is_valid_type() const -> bool {
    return static_cast<std::uint8_t>(request_type) < n_xfr_request_types;
  }

  [[nodiscard]] auto
  is_intervals_request() const -> bool {
    return request_type == request_type_code::intervals ||
           request_type == request_type_code::intervals_covered;
  }

  [[nodiscard]] auto
  is_bins_request() const -> bool {
    return request_type == request_type_code::bins ||
           request_type == request_type_code::bins_covered;
  }

  [[nodiscard]] auto
  is_windows_request() const -> bool {
    return request_type == request_type_code::windows ||
           request_type == request_type_code::windows_covered;
  }

  [[nodiscard]] auto
  is_covered_request() const -> bool {
    return request_type == request_type_code::intervals_covered ||
           request_type == request_type_code::bins_covered ||
           request_type == request_type_code::windows_covered;
  }

  [[nodiscard]] auto
  tostring() const -> std::string {
    std::string s;
    s +=
      std::format("{}\t{}\t{}", to_string(request_type), index_hash, aux_value);
    for (const auto &methylome_name : methylome_names)
      s += std::format("\t{}", methylome_name);
    return std::format("{}\n", s);
  }
};

[[nodiscard]] auto
compose(request_buffer &buf, const request &req) -> std::error_code;

[[nodiscard]] auto
parse(const request_buffer &buf, request &req) -> std::error_code;

}  // namespace transferase

template <>
struct std::formatter<transferase::request> : std::formatter<std::string> {
  auto
  format(const transferase::request &r, auto &ctx) const {
    std::string s;
    s += std::format("{}\t{}\t{}", to_string(r.request_type), r.index_hash,
                     r.aux_value);
    for (const auto &methylome_name : r.methylome_names)
      s += std::format("\t{}", methylome_name);
    return std::formatter<std::string>::format(s, ctx);
  }
};

// request error code
enum class request_error_code : std::uint8_t {
  ok = 0,
  parse_error_request_type = 1,
  parse_error_index_hash = 2,
  parse_error_aux_value = 3,
  parse_error_methylome_names = 4,
  error_reading_query = 5,
  request_too_large = 6,
};

template <>
struct std::is_error_code_enum<request_error_code> : public std::true_type {};

struct request_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override {return "request_error_code";}
  auto message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "error parsing request_type"s;
    case 2: return "error parsing index_hash"s;
    case 3: return "error parsing aux_value"s;
    case 4: return "error parsing methylome names"s;
    case 5: return "error reading query"s;
    case 6: return "request too large"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(request_error_code e) -> std::error_code {
  static auto category = request_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // LIB_REQUEST_HPP_
