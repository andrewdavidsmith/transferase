// SPDX-License-Identifier: MIT; Copyright 2026 Andrew D Smith

#include "request.hpp"

#include "request_impl.hpp"
#include "request_type_code.hpp"  // IWYU pragma: keep

#include <algorithm>
#include <cassert>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <format>
#include <iterator>
#include <ranges>  // IWYU pragma: keep
#include <string>
#include <string_view>

namespace transferase {

std::uint32_t request::max_intervals = request::max_intervals_default;
std::uint32_t request::min_bin_size = request::min_bin_size_default;
std::uint32_t request::min_window_size = request::min_window_size_default;
std::uint32_t request::min_window_step = request::min_window_step_default;

[[nodiscard]] STATIC INLINE auto
compose(char *buffer, const std::size_t size, const request &req)
  -> std::error_code {
  // ADS: use to_chars here
  const auto s = req.tostring();
  if (std::size(s) >= request_buffer_size)
    return request_error_code::request_too_large;

  assert(std::size(s) < size);

  // std::ranges::in_out_result
  auto data_end = std::ranges::copy(s, buffer);
  if (std::distance(buffer, data_end.out) == static_cast<std::ptrdiff_t>(size))
    return std::make_error_code(std::errc::result_out_of_range);
  *data_end.out = '\0';  // enables strlen, convenient for debugging
  return std::error_code{};
}

[[nodiscard]] STATIC inline auto
parse(const std::string_view buffer, request &req) -> std::error_code {
  static constexpr auto delim = '\t';
  static constexpr auto term = '\n';

  req = request{};  // reset everything

  auto cursor = std::cbegin(buffer);
  const auto last = std::cend(buffer);

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
  cursor = std::next(cursor);
  {
    const auto [ptr, ec] = std::from_chars(cursor, last, req.index_hash);
    if (ec != std::errc{})
      return request_error_code::parse_error_index_hash;
    cursor = ptr;
  }

  // aux value (either n_intervals or bin_size)
  if (*cursor != delim)
    return request_error_code::parse_error_aux_value;
  cursor = std::next(cursor);
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
    cursor = std::next(cursor);  // move beyond delim
    if (*cursor == delim)        // two delims in a row not allowed
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
  return compose(std::data(buf), std::size(buf), req);
}

[[nodiscard]] auto
parse(const request_buffer &buf, request &req) -> std::error_code {
  return parse(std::string_view(std::data(buf), std::size(buf)), req);
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

// parse(const char *buffer, const char *last, request &req) -> std::error_code
// { +parse(const std::string_view buffer, request &req) -> std::error_code {
//    static constexpr auto delim = '\t';
//    static constexpr auto term = '\n';

//    req = request{};  // reset everything

// -  auto cursor = buffer;
// +  auto cursor = std::cbegin(buffer);
// +  const auto last = std::cend(buffer);

//    // request type
//    {
// @@ -136,7 +138,7 @@ compose(request_buffer &buf,  // cppcheck-suppress
// constParameterReference

//  [[nodiscard]] auto
//  parse(const request_buffer &buf, request &req) -> std::error_code {
// -  return parse(std::data(buf), std::data(buf) + std::size(buf), req);
// +  return parse(std::string_view(std::data(buf), std::size(buf)), req);
//  }
