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

#include <config_file_utils.hpp>

#include <boost/describe.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

using namespace transferase;  // NOLINT

struct test_struct {
  int int_member{};
  std::string string_member;
  BOOST_DESCRIBE_CLASS(test_struct, (), (int_member, string_member), (), ())
};

[[nodiscard]] static auto
generate_complex_config(const std::map<std::string, std::string> &key_vals)
  -> std::string {
  std::string config;
  for (const auto &[key, val] : key_vals)
    config += key + " = " + val + "\n";
  return config;
}

TEST(config_format_test, format_as_config) {
  static constexpr auto expected = R"test(int-member = 42
string-member = example
)test";
  test_struct t{42, "example"};
  EXPECT_EQ(format_as_config(t), expected);
}

TEST(config_format_test, assign_member) {
  test_struct t{};
  assign_member(t, "int_member", "42");
  assign_member(t, "string_member", "example");
  EXPECT_EQ(t.int_member, 42);
  EXPECT_EQ(t.string_member, "example");
}

TEST(config_format_test, write_config_file) {
  static constexpr auto config_file = "complex_config.ini";
  static constexpr auto expected = R"test(int-member = 42
string-member = example
)test";
  test_struct t{42, "example"};
  const auto error = write_config_file(t, config_file);
  EXPECT_FALSE(error);

  std::ifstream in(config_file);
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
  EXPECT_EQ(content, expected);

  if (std::filesystem::exists(config_file)) {
    const bool remove_ok = std::filesystem::remove(config_file);
    EXPECT_TRUE(remove_ok);
  }
}

TEST(config_format_test, parse_complex_config) {
  static constexpr auto config_file = "complex_config.ini";
  std::map<std::string, std::string> key_vals = {
    {"int_member", "42"},
    {"string_member", "complex_example"},
    {"invalid_key", "1234"}};
  const auto config = generate_complex_config(key_vals);

  std::ofstream out(config_file);
  out << config;
  out.close();

  test_struct t{};
  std::error_code error;
  parse_config_file(t, config_file, error);

  EXPECT_FALSE(error);
  EXPECT_EQ(t.int_member, 42);
  EXPECT_EQ(t.string_member, "complex_example");

  if (std::filesystem::exists(config_file)) {
    const bool remove_ok = std::filesystem::remove(config_file);
    EXPECT_TRUE(remove_ok);
  }
}

TEST(config_format_test, parse_config_with_missing_values) {
  static constexpr auto config_file = "missing_values_config.ini";
  const std::map<std::string, std::string> key_vals = {
    {"int_member", ""}, {"string_member", "example"}};
  std::string config = generate_complex_config(key_vals);

  std::ofstream out(config_file);
  out << config;
  out.close();

  test_struct t{};
  std::error_code error;
  parse_config_file(t, config_file, error);

  EXPECT_TRUE(error);
  if (std::filesystem::exists(config_file)) {
    const bool remove_ok = std::filesystem::remove(config_file);
    EXPECT_TRUE(remove_ok);
  }
}

TEST(config_format_test, parse_config_with_invalid_keys) {
  static constexpr auto config_file = "invalid_keys_config.ini";
  std::map<std::string, std::string> key_vals = {{"int_member", "42"},
                                                 {"invalid_key", "1234"},
                                                 {"string_member", "example"}};
  std::string config = generate_complex_config(key_vals);

  std::ofstream out(config_file);
  out << config;
  out.close();

  test_struct t{};
  std::error_code error;
  parse_config_file(t, config_file, error);

  EXPECT_FALSE(error);
  EXPECT_EQ(t.int_member, 42);
  EXPECT_EQ(t.string_member, "example");
  if (std::filesystem::exists(config_file)) {
    const bool remove_ok = std::filesystem::remove(config_file);
    EXPECT_TRUE(remove_ok);
  }
}

TEST(config_format_test, parse_config_with_special_characters) {
  static constexpr auto config_file = "special_characters_config.ini";
  std::map<std::string, std::string> key_vals = {
    {"int_member", "42"},
    {"string_member", "example_with_special_chars!@#$%^&*()"}};
  std::string config = generate_complex_config(key_vals);

  std::ofstream out(config_file);
  out << config;
  out.close();

  test_struct t{};
  std::error_code error;
  parse_config_file(t, config_file, error);

  EXPECT_FALSE(error);
  EXPECT_EQ(t.int_member, 42);
  EXPECT_EQ(t.string_member, "example_with_special_chars!@#$%^&*()");
  if (std::filesystem::exists(config_file)) {
    const bool remove_ok = std::filesystem::remove(config_file);
    EXPECT_TRUE(remove_ok);
  }
}

TEST(config_format_test, parse_config_with_empty_file) {
  static constexpr auto config_file = "empty_config.ini";
  std::ofstream out(config_file);
  out.close();

  test_struct t{};
  std::error_code error;
  parse_config_file(t, config_file, error);

  EXPECT_FALSE(error);
  EXPECT_EQ(t.int_member, 0);      // default int value
  EXPECT_EQ(t.string_member, "");  // default string value
  if (std::filesystem::exists(config_file)) {
    const bool remove_ok = std::filesystem::remove(config_file);
    EXPECT_TRUE(remove_ok);
  }
}

TEST(config_format_test, parse_config_with_whitespace) {
  static constexpr auto config_file = "whitespace_config.ini";
  std::map<std::string, std::string> key_vals = {{"int_member", "42"},
                                                 {"string_member", "example"}};
  std::string config = generate_complex_config(key_vals);
  config = "   \n" + config + "   \n";

  std::ofstream out(config_file);
  out << config;
  out.close();

  test_struct t{};
  std::error_code error;
  parse_config_file(t, config_file, error);

  EXPECT_FALSE(error);
  EXPECT_EQ(t.int_member, 42);
  EXPECT_EQ(t.string_member, "example");
  if (std::filesystem::exists(config_file)) {
    const bool remove_ok = std::filesystem::remove(config_file);
    EXPECT_TRUE(remove_ok);
  }
}
