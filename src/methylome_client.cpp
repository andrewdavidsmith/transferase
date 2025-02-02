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

#include "methylome_client.hpp"

#include "client_config.hpp"

#include <algorithm>
#include <cctype>  // for std::isgraph
#include <cerrno>  // for errno
#include <fstream>
#include <iterator>  // for std::size, std::begin, std::cend
#include <ranges>
#include <string>
#include <system_error>
#include <tuple>

[[nodiscard]] inline auto
rlstrip(const std::string &s) noexcept -> std::string {
  constexpr auto is_graph = [](const unsigned char c) {
    return std::isgraph(c);
  };
  const auto start = std::ranges::find_if(s, is_graph);
  auto stop = std::begin(std::ranges::find_last_if(s, is_graph));
  if (stop != std::cend(s))
    ++stop;
  return std::string(start, stop);
}

namespace transferase {

[[nodiscard]] auto
split_equals(const std::string &line, std::error_code &error) noexcept
  -> std::tuple<std::string, std::string> {
  static constexpr auto delim = '=';
  const auto eq_pos = line.find(delim);
  if (eq_pos >= std::size(line)) {
    error = methylome_client_error_code::error_reading_config_file;
    return {std::string{}, std::string{}};
  }
  const std::string key{rlstrip(line.substr(0, eq_pos))};
  const std::string value{rlstrip(line.substr(eq_pos + 1))};
  if (std::size(key) == 0 || std::size(value) == 0) {
    error = methylome_client_error_code::error_reading_config_file;
    return {std::string{}, std::string{}};
  }

  return {key, value};
}

[[nodiscard]] auto
methylome_client::get_default(std::error_code &error) noexcept
  -> methylome_client {
  const auto config_file = client_config::get_config_file_default(error);
  if (error)
    // error from get_config_file_default
    return {};

  // read the config file
  std::ifstream in(config_file);
  if (!in) {
    error = std::make_error_code(std::errc(errno));
    return {};
  }

  std::string hostname;
  std::string port_number;

  std::string line;
  while (getline(in, line)) {
    line = rlstrip(line);
    if (line[0] == '#')
      continue;
    const auto [key, value] = split_equals(line, error);
    if (error)
      return {};
    if (key == "hostname")
      hostname = value;
    if (key == "port")
      port_number = value;
  }

  if (hostname.empty() || port_number.empty()) {
    error = methylome_client_error_code::required_config_values_not_found;
    return {};
  }

  methylome_client ms;
  ms.hostname = hostname;
  ms.port_number = port_number;

  return ms;
}

}  // namespace transferase
