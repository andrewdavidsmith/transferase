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

#include "level_container.hpp"
#include "level_element.hpp"

#include <cerrno>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace transferase {

[[nodiscard]] static inline auto
get_n_columns(const std::string &line) -> std::uint32_t {
  return std::ranges::count(line, '\t') + 1;
}

[[nodiscard]] static inline auto
parse_line(const std::string &line) -> std::vector<level_element_t> {
  std::vector<level_element_t> v;
  std::uint32_t word_count{};
  std::uint32_t val{};
  transferase::level_element_t elem{};

  std::istringstream iss(line);
  while (iss >> val) {
    if (word_count % 2 == 0)
      elem.n_meth = val;
    if (word_count % 2 == 1) {
      elem.n_unmeth = val;
      v.push_back(elem);
    }
    ++word_count;
  }
  if (word_count % 2 == 1)
    return {};
  return v;
}

[[nodiscard]] auto
read_level_container(const std::string &filename,
                        std::error_code &error) noexcept
  -> level_container<level_element_t> {
  std::ifstream in(filename);
  if (!in) {
    error = std::make_error_code(std::errc(errno));
    return {};
  }

  std::uint32_t n_cols = 0;
  std::uint32_t n_rows = 0;

  std::vector<std::vector<level_element_t>> v;
  std::string line;
  while (getline(in, line)) {
    if (n_cols == 0) {
      n_cols = get_n_columns(line) / 2;
      v.resize(n_cols);
    }
    const auto row = parse_line(line);
    if (row.empty()) {
      error = std::make_error_code(std::errc::invalid_argument);
      return {};
    }
    if (std::size(row) != n_cols) {
      error = std::make_error_code(std::errc::invalid_argument);
      return {};
    }
    for (std::uint32_t i = 0; i < n_cols; ++i)
      v[i].push_back(row[i]);
    ++n_rows;
  }
  return level_container<level_element_t>(n_rows, n_cols);
}

[[nodiscard]] auto
read_level_container_covered(
  [[maybe_unused]] const std::string &filename,
  [[maybe_unused]] std::error_code &error) noexcept
  -> level_container<level_element_covered_t> {
  return level_container<level_element_covered_t>();
}

}  // namespace transferase
