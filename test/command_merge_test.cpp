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

#include <command_merge.hpp>

#include "unit_test_utils.hpp"

#include <methylome_data.hpp>
#include <methylome_metadata.hpp>

#include <gtest/gtest.h>

#include <algorithm>  // for std::equal
#include <array>
#include <cstdlib>  // for EXIT_SUCCESS
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>

TEST(command_merge_test, basic_local_test) {
  // Input files for test
  static constexpr auto methylome_directory = "data/lutions/methylomes";
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto methylome_names = std::array{
    "eFlareon_brain"sv,
    "eFlareon_tail"sv,
    "eFlareon_ear"sv,
  };
  // Output filename and expected output
  static constexpr auto output_directory = "data/lutions";
  static constexpr auto merged_name = "eFlareon_merged";
  // ADS: bad idea to fully specify the name of this file with suffix
  static constexpr auto expected_output_data_file =
    "data/lutions/eFlareon_merged_expected.m16";

  // Define command line arguments
  const char *command_argv[] = {
    // clang-format off
    "merge",
    "-o",
    output_directory,
    "-d",
    methylome_directory,
    "-m",
    methylome_names[0].data(),
    methylome_names[1].data(),
    methylome_names[2].data(),
    "-n",
    merged_name,
    "-v",
    "debug",
    // clang-format on
  };
  const int command_argc = sizeof(command_argv) / sizeof(command_argv[0]);

  // Run the main function
  const int result =
    command_merge_main(command_argc, const_cast<char **>(command_argv));
  EXPECT_EQ(result, EXIT_SUCCESS);

  std::error_code ec;

  const auto output_meta_fn =
    compose_methylome_metadata_filename(output_directory, merged_name);
  const auto meta_exists = std::filesystem::exists(output_meta_fn, ec);
  EXPECT_FALSE(ec);
  EXPECT_TRUE(meta_exists);
  if (std::filesystem::exists(output_meta_fn, ec))
    std::filesystem::remove(output_meta_fn, ec);
  EXPECT_FALSE(ec);

  const auto output_data_fn =
    compose_methylome_data_filename(output_directory, merged_name);
  const auto data_exists = std::filesystem::exists(output_data_fn, ec);
  EXPECT_FALSE(ec);
  EXPECT_TRUE(data_exists);
  EXPECT_TRUE(files_are_identical(output_data_fn, expected_output_data_file));
  if (std::filesystem::exists(output_data_fn, ec))
    std::filesystem::remove(output_meta_fn, ec);
  EXPECT_FALSE(ec);
}
