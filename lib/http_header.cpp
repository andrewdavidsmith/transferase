// SPDX-License-Identifier: MIT; Copyright 2026 Andrew D Smith

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

http_header::http_header(const std::string &header_block) {
  const auto unview = [](const auto r) {
    return std::string(std::cbegin(r), std::cend(r));
  };
  for (const auto &line_view : header_block | std::views::split('\n')) {
    if (line_view.empty())
      continue;
    const auto line = unview(line_view);

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
  http_header(std::string(asio::buffers_begin(std::data(data)),
                          asio::buffers_begin(std::data(data)) +
                            static_cast<std::ptrdiff_t>(size))) {}

auto
http_header::tostring() const -> std::string {
  return std::format("status-code: {}\n"
                     "last-modified: {}\n"
                     "content-length: {}",
                     status_code, last_modified, content_length);
}

};  // namespace transferase
