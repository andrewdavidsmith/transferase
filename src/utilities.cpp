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

#include "logger.hpp"

#include <filesystem>  // for std::filesystem::path, std::filesystem::exists
#include <string>
#include <tuple>

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

auto
validate_output_directory(const std::string &dirname,
                          std::error_code &error) -> void {
  auto &lgr = transferase::logger::instance();
  const bool dir_exists = std::filesystem::exists(dirname, error);
  if (error) {
    lgr.error("Filesystem error {}: {}", dirname, error);
    return;
  }
  if (!dir_exists) {
    const bool dirs_ok = std::filesystem::create_directories(dirname, error);
    if (error) {
      lgr.error("Failed to create directory {}: {}", dirname, error);
      return;
    }
    const auto status = dirs_ok ? "created" : "exists";
    lgr.debug("Output directory {}: {}", dirname, status);
  }
}
