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

#include "utilities.hpp"

#include <cerrno>
#include <cstdlib>     // for std::getenv
#include <filesystem>  // for std::filesystem::path, std::filesystem::exists
#include <fstream>
#include <string>

namespace transferase {

[[nodiscard]] auto
get_server_config_dir_default(std::error_code &ec) -> std::string {
  static const auto config_dir_rhs =
    std::filesystem::path(".config/transferase");
  static const auto env_home = std::getenv("HOME");
  if (!env_home) {
    ec = std::make_error_code(std::errc{errno});
    return {};
  }
  const std::filesystem::path config_dir = env_home / config_dir_rhs;
  return config_dir;
}

}  // namespace transferase

[[nodiscard]] auto
split_equals(const std::string &line, std::error_code &error) noexcept
  -> std::tuple<std::string, std::string> {
  const auto key_ok = [](const unsigned char x) { return std::isgraph(x); };
  static constexpr auto delim = '=';
  const auto eq_pos = line.find(delim);
  if (eq_pos >= std::size(line)) {
    error = std::make_error_code(std::errc::invalid_argument);
    return {std::string{}, std::string{}};
  }
  const std::string key{rlstrip(line.substr(0, eq_pos))};
  if (!std::ranges::all_of(key, key_ok)) {
    error = std::make_error_code(std::errc::invalid_argument);
    return {std::string{}, std::string{}};
  }
  const std::string value{rlstrip(line.substr(eq_pos + 1))};
  if (std::size(key) == 0 || std::size(value) == 0) {
    error = std::make_error_code(std::errc::invalid_argument);
    return {std::string{}, std::string{}};
  }
  return {key, value};
}

[[nodiscard]] auto
clean_path(const std::string &s, std::error_code &ec) -> std::string {
  auto p = std::filesystem::absolute(s, ec);
  if (ec)
    return {};
  p = std::filesystem::weakly_canonical(p, ec);
  if (ec)
    return {};
  return p.string();
}

[[nodiscard]]
auto
check_output_file(const std::string &filename) -> std::error_code {
  std::error_code ec;
  // get the full name
  const auto canonical = std::filesystem::weakly_canonical(filename, ec);
  if (ec)
    return ec;

  // check if this is a directory
  const bool already_exists = std::filesystem::exists(canonical);
  if (already_exists) {
    const bool is_dir = std::filesystem::is_directory(canonical, ec);
    if (ec)
      return ec;
    if (is_dir)
      return std::error_code{output_file_error_code::is_a_directory};
    return {};
  }

  // if it doesn't already exist, test if it's writable
  // if (!already_exists) // ADS: would be redundant for now
  {
    std::ofstream out_test(canonical);
    if (!out_test)
      ec = output_file_error_code::failed_to_open;
    [[maybe_unused]] std::error_code unused{};
    const bool out_test_exits = std::filesystem::exists(canonical, unused);
    if (out_test_exits) {
      [[maybe_unused]]
      const bool removed = std::filesystem::remove(canonical, unused);
    }
    return ec;
  }
  // ADS: somehow need to check if an existing file can be written
  // without modifying it??

  return {};
}

[[nodiscard]] auto
parse_config_file(const std::string &filename, std::error_code &error) noexcept
  -> std::vector<std::tuple<std::string, std::string>> {
  std::ifstream in(filename);
  if (!in) {
    error = std::make_error_code(std::errc(errno));
    return {};
  }

  std::vector<std::tuple<std::string, std::string>> key_val;
  std::string line;
  while (getline(in, line)) {
    line = rlstrip(line);
    // ignore empty lines and those beginning with '#'
    if (!line.empty() && line[0] != '#') {
      const auto [k, v] = split_equals(line, error);
      if (error)
        return {};
      key_val.emplace_back(k, v);
    }
  }
  return key_val;
}
