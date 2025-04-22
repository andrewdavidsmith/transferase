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

#ifndef LIB_HTTP_CLIENT_HPP_
#define LIB_HTTP_CLIENT_HPP_

#include "http_header.hpp"

#include <chrono>
#include <string>
#include <system_error>
#include <tuple>
#include <variant>

namespace transferase {

[[nodiscard]] auto
download_http(
  const std::string &host, const std::string &port, const std::string &target,
  const std::string &outfile, const std::chrono::microseconds timeout,
  const bool show_progress = false) -> std::tuple<http_header, std::error_code>;

[[nodiscard]] auto
download_header_http(const std::string &host, const std::string &port,
                     const std::string &target,
                     const std::chrono::microseconds timeout) -> http_header;

};  // namespace transferase

#endif  // LIB_HTTP_CLIENT_HPP_
