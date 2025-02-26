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

#ifndef CLI_TESTS_UNIT_TEST_UTILS_CLI_HPP_
#define CLI_TESTS_UNIT_TEST_UTILS_CLI_HPP_

#include <string>
#include <system_error>

[[nodiscard]] auto
files_are_identical_cli(const std::string &, const std::string &) -> bool;

[[nodiscard]] auto
generate_temp_filename_cli(const std::string &prefix,
                           const std::string &suffix = "") -> std::string;

[[nodiscard]] auto
generate_unique_dir_name_cli() -> std::string;

auto
remove_directories_cli(const std::string &dirname,
                       std::error_code &error) -> void;

#endif  // CLI_TESTS_UNIT_TEST_UTILS_CLI_HPP_
