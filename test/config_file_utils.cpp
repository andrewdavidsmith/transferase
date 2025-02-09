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

#include "unit_test_utils.hpp"

#include <boost/beast.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <utility>

using namespace transferase;  // NOLINT

#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <format>
#include <fstream>
#include <gtest/gtest.h>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

template <typename T>
[[nodiscard]] inline auto
format_as_config(const T &t) -> std::string {
  using members =
    boost::describe::describe_members<T, boost::describe::mod_any_access>;
  std::string r;
  boost::mp11::mp_for_each<members>([&](const auto &member) {
    std::string name(member.name);
    std::ranges::replace(name, '_', '-');
    const auto value = std::format("{}", t.*member.pointer);
    if (!value.empty())
      r += std::format("{} = {}\n", name, value);
    else
      r += std::format("# {} =\n", name);
  });
  return r;
}

inline auto
assign_member_impl(auto &t, const std::string &value) {
  std::istringstream(value) >> t;
}

inline auto
assign_member(auto &t, const std::string_view name, const auto &value) {
  namespace bd = boost::describe;
  using T = std::remove_cvref_t<decltype(t)>;
  using Md = bd::describe_members<T, bd::mod_any_access>;
  boost::mp11::mp_for_each<Md>([&](const auto &D) {
    if (name == D.name)
      assign_member_impl(t.*D.pointer, value);
  });
}

inline auto
parse_config_file(auto &t, const std::string &filename,
                  std::error_code &error) -> void {
  const auto key_vals = parse_config_file_as_key_val(filename, error);
  if (error)
    return;
  // attempt to assign each value to some variable of the class
  for (const auto &[key, val] : key_vals) {
    std::string name(key);
    std::ranges::replace(name, '-', '_');
    assign_member(t, name, val);
  }
}

[[nodiscard]] inline auto
write_config_file(const auto &obj,
                  const std::string &config_file) -> std::error_code {
  std::ofstream out(config_file);
  if (!out)
    return std::make_error_code(std::errc(errno));
  std::print(out, "{}", format_as_config(obj));
  if (!out)
    return std::make_error_code(std::errc(errno));
  return std::error_code{};
}

struct TestStruct {
  int int_member;
  std::string string_member;
  BOOST_DESCRIBE_CLASS(TestStruct, (), (int_member, string_member), (), ())
};

TEST(ConfigFormatTest, FormatAsConfig) {
  TestStruct t{42, "example"};
  std::string expected = "int-member = 42\nstring-member = example\n";
  EXPECT_EQ(format_as_config(t), expected);
}

TEST(ConfigFormatTest, AssignMember) {
  TestStruct t{};
  assign_member(t, "int_member", "42");
  assign_member(t, "string_member", "example");
  EXPECT_EQ(t.int_member, 42);
  EXPECT_EQ(t.string_member, "example");
}

TEST(ConfigFormatTest, WriteConfigFile) {
  TestStruct t{42, "example"};
  std::string config_file = "test_config.ini";
  auto error = write_config_file(t, config_file);
  EXPECT_FALSE(error);

  std::ifstream in(config_file);
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
  std::string expected = "int-member = 42\nstring-member = example\n";
  EXPECT_EQ(content, expected);
}

int
main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
