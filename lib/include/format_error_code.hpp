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

#ifndef LIB_INCLUDE_FORMAT_ERROR_CODE_HPP_
#define LIB_INCLUDE_FORMAT_ERROR_CODE_HPP_

#include <boost/system.hpp>

#include <cstdint>
#include <format>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

template <>
struct std::formatter<std::error_code> : std::formatter<std::string> {
  auto
  format(const std::error_code &e, std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}", e.message());
  }
};

template <>
struct std::formatter<boost::system::error_code> : std::formatter<std::string> {
  auto
  format(const boost::system::error_code &e, std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}", e.message());
  }
};

#endif  // LIB_INCLUDE_FORMAT_ERROR_CODE_HPP_
