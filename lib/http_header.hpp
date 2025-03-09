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

#ifndef LIB_HTTP_HEADER_HPP_
#define LIB_HTTP_HEADER_HPP_

#include <asio.hpp>

#include <cstdlib>
#include <string>

namespace transferase {

struct http_header {
  std::string status_line;       // HTTP/2 200
  std::string status_code;       // 200
  std::string last_modified;     // last-modified: Sun, 02 Feb 2025 20:10:46 GMT
  std::size_t content_length{};  // content-length: 117607180

  http_header() = default;
  explicit http_header(const std::string &header_block);
  http_header(const asio::streambuf &data, const std::size_t size);
  auto
  tostring() const -> std::string;
};

}  // namespace transferase

#endif  // LIB_HTTP_HEADER_HPP_
