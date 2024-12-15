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

#ifndef SRC_UTILITIES_HPP_
#define SRC_UTILITIES_HPP_

/*
  Functions declared here are used by multiple source files
 */

#include <chrono>
#include <filesystem>
#include <format>
#include <iterator>  // for std::size
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <variant>  // IWYU pragma: keep

struct compose_result {
  char *ptr{};
  std::error_code error{};
};

struct parse_result {
  const char *ptr{};
  std::error_code error{};
};

[[nodiscard]] inline auto
duration(const auto start, const auto stop) {
  const auto d = stop - start;
  return std::chrono::duration_cast<std::chrono::duration<double>>(d).count();
}

template <>
struct std::formatter<std::filesystem::path> : std::formatter<std::string> {
  auto
  format(const std::filesystem::path &p, std::format_context &ctx) const {
    return std::formatter<std::string>::format(p.string(), ctx);
  }
};

[[nodiscard]] auto
get_mxe_config_dir_default(std::error_code &ec) -> std::string;

[[nodiscard]] inline auto
strip(char const *const x) -> const std::string_view {
  const std::string s{x};
  const auto start = s.find_first_not_of("\n\r");
  return std::string_view(x + start, x + std::size(s));
}

[[nodiscard]] auto
get_username() -> std::tuple<std::string, std::error_code>;

[[nodiscard]] auto
get_time_as_string() -> std::string;

#endif  // SRC_UTILITIES_HPP_
