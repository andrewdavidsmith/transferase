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

#ifndef SRC_HASH_HPP_
#define SRC_HASH_HPP_

#include <zlib.h>

#include <cstdint>
#include <string>
#include <system_error>

namespace xfrase {

[[nodiscard]] inline auto
get_adler(const auto &data, const auto data_size) -> std::uint64_t {
  return adler32_z(0, reinterpret_cast<const std::uint8_t *>(data), data_size);
}

[[nodiscard]] inline auto
update_adler(const auto previous, const auto &data,
             const auto data_size) -> std::uint64_t {
  // ADS: hopefull this function won't be needed after some refactoring
  const auto adler = get_adler(data, data_size);
  return adler32_combine(previous, adler, data_size);
}

// ADS: this function should be replaced by one that can operate on a
// the data as though it were serealized but without reading the file
[[nodiscard]] auto
get_adler(const std::string &filename) -> std::uint64_t;

// ADS: this function should be replaced by one that can operate on a
// the data as though it were serealized but without reading the file
[[nodiscard]] auto
get_adler(const std::string &filename, std::error_code &ec) -> std::uint64_t;

}  // namespace xfrase

#endif  // SRC_HASH_HPP_
