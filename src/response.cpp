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

#include "response.hpp"
#include "mc16_error.hpp"

#include <cstring>
#include <format>
#include <string>

using std::string;

auto
response::to_buffer() -> std::error_condition {
  const int v = static_cast<int>(status.value());
  static constexpr auto sz1 = sizeof(int);
  static constexpr auto sz2 = sizeof(uint32_t);
  std::memcpy(buf.data(), reinterpret_cast<const char *>(&v), sz1);
  std::memcpy(buf.data() + sz1, reinterpret_cast<const char *>(&methylome_size), sz2);
  return status;
}

auto
response::from_buffer() -> std::error_condition {
  static constexpr auto sz1 = sizeof(int);
  static constexpr auto sz2 = sizeof(uint32_t);
  int v{};
  std::memcpy(reinterpret_cast<char *>(&v), buf.data(), sz1);
  status = make_error_condition(static_cast<server_response_code>(v));
  std::memcpy(reinterpret_cast<char *>(&methylome_size), buf.data() + sz1, sz2);
  return status;
}

auto
response::not_found() -> response {
  return {
    {}, server_response_code::methylome_not_found, 0, {}};
}

auto
response::bad_request() -> response {
  return {{}, server_response_code::bad_request, 0, {}};
}

auto
response::summary() const -> string {
  static constexpr auto fmt = R"({}: "{}"\nmethylome_size: {})";
  return std::format(fmt, status.category().name(), status.message(),
                     methylome_size);
}

auto
response::summary_serial() const -> string {
  static constexpr auto fmt = R"({{"{}": "{}", "methylome_size": {}}})";
  return std::format(fmt, status.category().name(), status.message(),
                     methylome_size);
}
