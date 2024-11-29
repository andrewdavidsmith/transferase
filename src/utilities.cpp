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

#include <zlib.h>

#include <algorithm>
#include <cassert>
#include <string>
#include <utility>
#include <vector>

[[nodiscard]] auto
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

// ADS: this function should be replaced by one that can operate on a
// the data as though it were serealized but without reading the file
[[nodiscard]] auto
get_adler(const std::string &filename) -> std::uint64_t {
  const auto filesize = std::filesystem::file_size(filename);
  std::vector<char> buf(filesize);
  std::ifstream in(filename);
  in.read(buf.data(), filesize);
  return adler32_z(0, reinterpret_cast<std::uint8_t *>(buf.data()), filesize);
}

// ADS: this function should be replaced by one that can operate on a
// the data as though it were serealized but without reading the file
[[nodiscard]] auto
get_adler(const std::string &filename, std::error_code &ec) -> std::uint64_t {
  const auto filesize = std::filesystem::file_size(filename, ec);
  if (ec)
    return 0;
  std::vector<char> buf(filesize);
  std::ifstream in(filename);
  if (!in) {
    ec = std::make_error_code(std::errc(errno));
    return 0;
  }
  if (!in.read(buf.data(), filesize)) {
    ec = std::make_error_code(std::errc(errno));
    return 0;
  }
  return adler32_z(0, reinterpret_cast<std::uint8_t *>(buf.data()), filesize);
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
