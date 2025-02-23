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

#include "unit_test_utils.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <random>
#include <string>

[[nodiscard]] auto
files_are_identical(const std::string &fn1, const std::string &fn2) -> bool {
  std::ifstream f1(fn1, std::ifstream::binary);
  if (!f1)
    return false;
  std::ifstream f2(fn2, std::ifstream::binary);
  if (!f2)
    return false;
  return std::equal(std::istreambuf_iterator<char>(f1), {},
                    std::istreambuf_iterator<char>(f2));
}

[[nodiscard]] auto
generate_temp_filename(const std::string &prefix,
                       const std::string &suffix) -> std::string {
  const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
  const auto filename =
    std::format("{}_{}{}", prefix, millis,
                (suffix.empty() || suffix[0] == '.') ? suffix : "." + suffix);
  return (std::filesystem::temp_directory_path() / filename).string();
}

[[nodiscard]] auto
generate_unique_dir_name() -> std::string {
  static constexpr auto test_dir_prefix = "test_dir_";
  static constexpr auto min_fn_suff = 1000;
  static constexpr auto max_fn_suff = 9999;
  // Generate a random string based on current time and random numbers
  const auto now = std::chrono::system_clock::now().time_since_epoch().count();
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(min_fn_suff, max_fn_suff);
  return (std::filesystem::temp_directory_path() /
          (test_dir_prefix + std::to_string(now) + "_" +
           std::to_string(dis(gen))))
    .string();
}

auto
remove_directories(const std::string &dirname, std::error_code &error) -> void {
  const auto dir_exists = std::filesystem::exists(dirname, error);
  if (error || !dir_exists)
    return;
  const auto dir_is_dir = std::filesystem::is_directory(dirname, error);
  if (error || !dir_is_dir)
    return;
  [[maybe_unused]] const bool remove_ok =
    std::filesystem::remove_all(dirname, error);
}
