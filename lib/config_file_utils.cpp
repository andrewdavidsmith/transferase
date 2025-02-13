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

#include "config_file_utils.hpp"

#include "utilities.hpp"

#include <fstream>
#include <string>
#include <system_error>
#include <tuple>
#include <variant>
#include <vector>

namespace transferase {

[[nodiscard]] auto
parse_config_file_as_key_val(const std::string &filename,
                             std::error_code &error) noexcept
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

}  // namespace transferase
