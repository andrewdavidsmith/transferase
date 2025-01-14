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

#include <command_config_argset.hpp>

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <format>
#include <iterator>  // for std::size
#include <string>

using namespace transferase;  // NOLINT

TEST(command_config_argset_test, run_success) {
  using std::string_literals::operator""s;
  const auto argv = std::array{
    // clang-format off
    "config",
    "-v",
    "debug",
    "-c",
    "config/transferase_client_config.toml",
    "-s",
     "example.com",
    "-p",
    "5000",
    "--assemblies",
    "hg38,mm39",
    // clang-format on
  };
  const int argc = sizeof(argv) / sizeof(argv[0]);

  EXPECT_EQ(argc, 11);
  EXPECT_EQ(argv[0], "config");

  command_config_argset args;
  // NOLINTBEGIN(cppcoreguidelines-pro-type-const-cast)
  const auto ec = args.parse(argc, const_cast<char **>(argv.data()), "usage"s,
                             "about"s, "description"s);
  // NOLINTEND(cppcoreguidelines-pro-type-const-cast)
  const std::filesystem::path config_file{
    "config/transferase_client_config.toml"};
  EXPECT_EQ(args.client_config_file, "config/transferase_client_config.toml");
  EXPECT_FALSE(ec);
}

TEST(command_config_argset_test, default_client_config_file) {
  using std::string_literals::operator""s;
  const auto argv = std::array{
    // clang-format off
    "config",
    "-v",
    "critical",
    "-s",
    "example.com",
    "-p",
    "5000",
    "--assemblies",
    "hg38,mm39",
    // clang-format on
  };
  const int argc = sizeof(argv) / sizeof(argv[0]);

  EXPECT_EQ(argc, 9);
  EXPECT_EQ(argv[0], "config");

  command_config_argset args;
  // NOLINTBEGIN(cppcoreguidelines-pro-type-const-cast)
  std::error_code ec = args.parse(argc, const_cast<char **>(argv.data()),
                                  "usage"s, "about"s, "description"s);
  // NOLINTEND(cppcoreguidelines-pro-type-const-cast)
  EXPECT_FALSE(ec);

  const auto default_config_dir = get_transferase_config_dir_default(ec);
  EXPECT_FALSE(ec);

  const auto default_config_file_path =
    std::format("{}/{}", default_config_dir,
                command_config_argset::default_config_filename);
  EXPECT_EQ(args.client_config_file, default_config_file_path);
}
