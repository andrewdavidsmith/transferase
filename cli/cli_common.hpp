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

#ifndef CLI_CLI_COMMON_HPP_
#define CLI_CLI_COMMON_HPP_

#include "CLI11/CLI11.hpp"

#include <string>
#include <vector>

static const int column_width_default = 30;

class transferase_formatter : public CLI::Formatter {
  static constexpr auto max_descr_width = 50;

public:
  auto
  make_option_desc(const CLI::Option *opt) const -> std::string override {
    static constexpr auto max_descr_width = 50;
    std::istringstream iss{opt->get_description()};
    const std::vector<std::string> words{
      std::istream_iterator<std::string>{iss}, {}};
    std::string r{words[0]};
    std::uint32_t width = std::size(words[0]);
    for (auto i = 1u; i < std::size(words); ++i) {
      if (width == 0 || width + std::size(words[i]) < max_descr_width) {
        r += ' ';
        ++width;
      }
      else {
        r += '\n';
        width = 0;
      }
      r += words[i];
      width += std::size(words[i]);
    }
    return r;
  }
};

#endif  // CLI_CLI_COMMON_HPP_
