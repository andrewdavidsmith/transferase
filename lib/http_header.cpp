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

#include "http_header.hpp"

#include <asio.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <iterator>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>

namespace transferase {

// trim from start (in place)
static inline auto
ltrim(std::string &s) {
  const auto no_space = [](const auto c) { return !std::isspace(c); };
  const auto e = std::find_if(std::cbegin(s), std::cend(s), no_space);
  s.erase(std::cbegin(s), e);
}

// trim from end (in place)
static inline auto
rtrim(std::string &s) -> void {
  const auto no_space = [](const auto c) { return !std::isspace(c); };
  const auto b = std::find_if(std::crbegin(s), std::crend(s), no_space);
  s.erase(b.base(), std::cend(s));
}

// trim from both ends (in place)
static inline auto
trim(std::string &s) -> void {
  rtrim(s);
  ltrim(s);
}

// split a string at the first colon
static inline auto
split_http_field(const std::string &s) -> std::tuple<std::string, std::string> {
  const auto colon = s.find(':');
  if (colon >= std::size(s) - 1)
    return {{}, {}};
  auto field_name = s.substr(0, colon);
  auto field_value = s.substr(colon + 1);
  trim(field_name);
  trim(field_value);
  if (field_value.empty())
    return {{}, {}};
  std::ranges::for_each(field_name, [](auto &c) { c = std::tolower(c); });
  return {field_name, field_value};
}

static inline auto
is_status_line(const std::string &line) -> bool {
  using std::string_view_literals::operator""sv;
  constexpr auto http_tag = "HTTP"sv;
  return line.starts_with(http_tag);
}

static inline auto
parse_status_line(const std::string &line, std::string &status_line,
                  std::string &status_code) {
  constexpr auto status_code_size = 3u;

  status_line = line;
  std::istringstream iss(line);
  std::string field_name;
  std::string field_value;
  if ((iss >> field_name >> field_value) &&
      std::size(field_value) == status_code_size &&
      std::ranges::all_of(field_value,
                          [](const auto c) { return std::isdigit(c); }))
    status_code = field_value;
}

http_header::http_header(const std::string &header_block) {
  const auto unview = [](const auto r) {
    return std::string(std::cbegin(r), std::cend(r));
  };
  for (const auto &line_view : header_block | std::views::split('\n')) {
    if (line_view.empty())
      continue;
    const auto line = unview(line_view);
    if (is_status_line(line)) {
      parse_status_line(line, status_line, status_code);
      continue;
    }

    const auto [field_name, field_value] = split_http_field(line);
    if (field_name.empty())
      continue;

    if (field_name == "content-length") {
      std::istringstream iss(field_value);
      std::size_t content_length_tmp{};
      if (iss >> content_length_tmp)
        content_length = content_length_tmp;
    }
    if (field_name == "last-modified")
      last_modified = field_value;
  }
}

http_header::http_header(const asio::streambuf &data, const std::size_t size) :
  http_header(std::string(asio::buffers_begin(data.data()),
                          asio::buffers_begin(data.data()) +
                            static_cast<std::ptrdiff_t>(size))) {}

auto
http_header::tostring() const -> std::string {
  return std::format("{}\n"
                     "status-code: {}\n"
                     "last-modified: {}\n"
                     "content-length: {}",
                     status_line, status_code, last_modified, content_length);
}

// auto
// parse_header(const std::string_view &header_block) -> void {
//   for (const auto &v : header_block | std::views::split('\n')) {
//     const auto line = std::string(std::cbegin(v), std::cend(v));
//     const auto colon_pos = line.find(':');
//     if (colon_pos == std::string::npos)
//       header.emplace("response", line);
//     else
//       header.emplace(line.substr(0, colon_pos), line.substr(colon_pos +
//       1));
//   }
// }

};  // namespace transferase
