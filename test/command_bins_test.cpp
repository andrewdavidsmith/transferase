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

#include <command_bins.hpp>

#include "unit_test_utils.hpp"

#include <gtest/gtest.h>

#include <cstdlib>  // for EXIT_SUCCESS
#include <filesystem>
#include <string>
#include <system_error>

TEST(command_bins_test, basic_local_test) {
  // Input files for test
  static constexpr auto index_directory = "data";
  static constexpr auto genome_name = "pAntiquusx";
  static constexpr auto methylome_directory = "data";
  static constexpr auto methylome_name = "SRX012346";
  // Output filename and expected output
  static constexpr auto output_file = "data/output_file.bed";
  static constexpr auto expected_output_file =
    "data/SRX012346_bin100_local.bed";

  // Define command line arguments
  // NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays)
  const char *command_argv[] = {
    // clang-format off
    "bins",
    "local",
    "-x",
    index_directory,
    "-g",
    genome_name,
    "-d",
    methylome_directory,
    "-m",
    methylome_name,
    "-o",
    output_file,
    "-b",
    "100",
    // clang-format on
  };
  // NOLINTEND(cppcoreguidelines-avoid-c-arrays)
  const int command_argc = sizeof(command_argv) / sizeof(command_argv[0]);

  // Run the main function
  // NOLINTBEGIN(cppcoreguidelines-pro-type-const-cast)
  const int result =
    command_bins_main(command_argc, const_cast<char **>(command_argv));
  // NOLINTEND(cppcoreguidelines-pro-type-const-cast)

  // Check that the output file is created
  EXPECT_EQ(result, EXIT_SUCCESS);
  std::error_code ignored_ec;
  EXPECT_TRUE(std::filesystem::exists(output_file, ignored_ec));
  EXPECT_TRUE(files_are_identical(output_file, expected_output_file));

  // Clean up: delete test files
  if (std::filesystem::exists(output_file)) {
    const auto remove_ok = std::filesystem::remove(output_file);
    EXPECT_TRUE(remove_ok);
  }
}
