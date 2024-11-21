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

/*
  Functions defined here are used by multiple source files
 */

#include "cpg_index.hpp"
#include "genomic_interval.hpp"
#include "methylome.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <ostream>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

using std::array;
using std::pair;
using std::string;
using std::to_chars;
using std::vector;

namespace rg = std::ranges;
namespace vs = std::views;

auto
get_mxe_config_dir_default(std::error_code &ec) -> std::string {
  static const auto config_dir_rhs = std::filesystem::path(".config/mxe");
  static const auto env_home = std::getenv("HOME");
  if (!env_home) {
    ec = std::make_error_code(std::errc{errno});
    return {};
  }
  const std::filesystem::path config_dir = env_home / config_dir_rhs;
  return config_dir;
}

/*
auto
check_mxe_config_dir(const std::string &dirname, std::error_code &ec) -> bool {
  const bool exists = std::filesystem::exists(dirname, ec);
  if (ec)
    return false;

  if (!exists) {
    ec = std::make_error_code(std::errc::no_such_file_or_directory);
    return false;
  }

  const bool is_dir = std::filesystem::is_directory(dirname, ec);
  if (ec)
    return false;
  if (!is_dir) {
    ec = std::make_error_code(std::errc::not_a_directory);
    return false;
  }

  return true;
}
*/
