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

#ifndef LIB_INCLUDE_ENVIRONMENT_UTILITIES_HPP_
#define LIB_INCLUDE_ENVIRONMENT_UTILITIES_HPP_

#include <string>
#include <system_error>
#include <tuple>

namespace transferase {

[[nodiscard]] auto
get_username() -> std::tuple<std::string, std::error_code>;

[[nodiscard]] auto
get_time_as_string() -> std::string;

[[nodiscard]] auto
get_hostname() -> std::tuple<std::string, std::error_code>;

[[nodiscard]] auto
get_version() -> std::string;

}  // namespace transferase

#endif  // LIB_INCLUDE_ENVIRONMENT_UTILITIES_HPP_
