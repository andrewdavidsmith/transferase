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

#include "request.hpp"

#include "request_impl.hpp"
#include "request_type_code.hpp"  // IWYU pragma: keep

#include <algorithm>
#include <cassert>
#include <cctype>    // for isalnum, std::isalnum
#include <charconv>  // std::from_chars
#include <format>
#include <iterator>  // for std::size, std::distance, std::iterator_traits
#include <ranges>    // IWYU pragma: keep
#include <string>

namespace transferase {

std::uint32_t request::max_intervals = request::max_intervals_default;
std::uint32_t request::min_bin_size = request::min_bin_size_default;
std::uint32_t request::min_window_size = request::min_window_size_default;
std::uint32_t request::min_window_step = request::min_window_step_default;

[[nodiscard]] STATIC INLINE auto
compose(char *first, char const *last, const request &req) -> std::error_code {
  // ADS: use to_chars here
  const std::string s = std::format("{}\n", req.tostring());
  if (std::size(s) >= request_buffer_size)
    return request_error_code::request_too_large;

#ifndef NDEBUG
  typedef std::iterator_traits<char *>::difference_type diff_type;
  const auto first_const = static_cast<char const *>(first);
  const auto dt_size_s = static_cast<diff_type>(std::size(s));
  assert(dt_size_s < std::distance(first_const, last));
#endif

  // std::ranges::in_out_result
  auto data_end = std::ranges::copy(s, first);
  if (data_end.out == last)
    return std::make_error_code(std::errc::result_out_of_range);
  *data_end.out = '\0';  // enables strlen, convenient for debugging
  return std::error_code{};
}

[[nodiscard]] STATIC inline auto
parse(char const *first, char const *last, request &req) -> std::error_code {
  static constexpr auto delim = '\t';
  static constexpr auto term = '\n';

  req = request{};  // reset everything

  auto cursor = first;

  // request type
  {
    std::underlying_type_t<request_type_code> tmp{};
    const auto [ptr, ec] = std::from_chars(cursor, last, tmp);
    if (ec != std::errc{})
      return request_error_code::parse_error_request_type;
    req.request_type = static_cast<request_type_code>(tmp);
    cursor = ptr;
  }

  // index hash
  if (*cursor != delim)
    return request_error_code::parse_error_index_hash;
  ++cursor;
  {
    const auto [ptr, ec] = std::from_chars(cursor, last, req.index_hash);
    if (ec != std::errc{})
      return request_error_code::parse_error_index_hash;
    cursor = ptr;
  }

  // aux value (either n_intervals or bin_size)
  if (*cursor != delim)
    return request_error_code::parse_error_aux_value;
  ++cursor;
  {
    const auto [ptr, ec] = std::from_chars(cursor, last, req.aux_value);
    if (ec != std::errc{})
      return request_error_code::parse_error_aux_value;
    cursor = ptr;
  }

  // methylome_names
  const auto ok_name = [](const auto x) { return std::isalnum(x) || x == '_'; };
  req.methylome_names.clear();
  while (true) {
    if (*cursor != delim)
      break;
    // didn't break; we have another methylome name
    ++cursor;              // move beyond delim
    if (*cursor == delim)  // two delims in a row not allowed
      break;
    // find where the methylome name ends
    const auto methylome_name_end = std::find_if_not(cursor, last, ok_name);
    if (methylome_name_end == last)
      return request_error_code::parse_error_methylome_names;
    req.methylome_names.emplace_back(cursor, methylome_name_end);
    cursor = methylome_name_end;
  }
  if (*cursor != term)
    return request_error_code::parse_error_methylome_names;

  return std::error_code{};
}

[[nodiscard]] auto
compose(request_buffer &buf,  // cppcheck-suppress constParameterReference
        const request &req) -> std::error_code {
  return compose(std::data(buf), std::data(buf) + std::size(buf), req);
}

[[nodiscard]] auto
parse(const request_buffer &buf, request &req) -> std::error_code {
  return parse(std::data(buf), std::data(buf) + std::size(buf), req);
}

[[nodiscard]] auto
request::summary() const -> std::string {
  // auto joined = methylome_names | std::views::transform([](const auto &r) {
  //                 return std::format("\"{}\"", r);
  //               }) |
  //               std::views::join_with(',') | std::ranges::to<std::string>();
  std::string joined = std::format("\"{}\"", methylome_names.front());
  for (auto i = 1u; i < std::size(methylome_names); ++i)
    joined += std::format(",\"{}\"", methylome_names[i]);
  return std::format(R"({{"request_type": {}, )"
                     R"("index_hash": {}, )"
                     R"("aux_value": {}, )"
                     R"("methylome_names": [{}]}})",
                     to_string(request_type), index_hash, aux_value, joined);
}

}  // namespace transferase
