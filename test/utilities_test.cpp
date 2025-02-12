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

#include <utilities.hpp>

#include <logger.hpp>

#include <gtest/gtest.h>

#include <string>
#include <system_error>
#include <utility>

using namespace transferase;  // NOLINT
using std::string_literals::operator""s;

TEST(utilities_test, clean_path_test) {
  static const auto relative = "./something"s;
  std::error_code error;
  const auto cleaned = clean_path(relative, error);
  EXPECT_FALSE(error);
  EXPECT_NE(relative, cleaned);
}

TEST(utilities_test, split_comma_test) {
  static const auto with_commas_ok = "a,b,c,d"s;
  static const auto with_commas_left = ",a,b,c,d"s;
  static const auto with_commas_right = "a,b,c,d,"s;
  static const auto with_commas_mid = "a,,b,c,d"s;

  auto s = split_comma(with_commas_ok);
  EXPECT_EQ(std::size(s), 4ul);

  s = split_comma(with_commas_left);
  EXPECT_EQ(std::size(s), 4ul);

  s = split_comma(with_commas_right);
  EXPECT_EQ(std::size(s), 4ul);

  s = split_comma(with_commas_mid);
  EXPECT_EQ(std::size(s), 4ul);
}

TEST(utilities_test, split_rlstrip_test) {
  static const auto tight = "asdf"s;
  static const auto with_left = "  asdf"s;
  static const auto with_right = "asdf  "s;

  auto s = rlstrip(tight);
  EXPECT_EQ(s, tight);

  s = rlstrip(with_left);
  EXPECT_EQ(s, tight);

  s = rlstrip(with_right);
  EXPECT_EQ(s, tight);
}

TEST(utilities_test, split_equals_test) {
  static const auto tight = "asdf = 1234"s;
  static const auto with_left = "  asdf = 1234"s;
  static const auto with_right = "asdf = 1234  "s;
  static const auto with_broken_key = "asdf asdf = 1234"s;
  static const auto with_broken_val = "asdf = 1234 1234"s;

  {
    std::error_code error;
    const auto [k, v] = split_equals(tight, error);
    EXPECT_FALSE(error);
    EXPECT_EQ(k, "asdf"s);
    EXPECT_EQ(v, "1234"s);
  }

  {
    std::error_code error;
    const auto [k, v] = split_equals(with_left, error);
    EXPECT_FALSE(error);
    EXPECT_EQ(k, "asdf"s);
    EXPECT_EQ(v, "1234"s);
  }

  {
    std::error_code error;
    const auto [k, v] = split_equals(with_right, error);
    EXPECT_FALSE(error);
    EXPECT_EQ(k, "asdf"s);
    EXPECT_EQ(v, "1234"s);
  }

  {
    std::error_code error;
    const auto [k, v] = split_equals(with_broken_key, error);
    EXPECT_TRUE(error);
  }

  {
    std::error_code error;
    const auto [k, v] = split_equals(with_broken_val, error);
    EXPECT_FALSE(error);
    EXPECT_EQ(k, "asdf"s);
    EXPECT_EQ(v, "1234 1234"s);
  }
}
