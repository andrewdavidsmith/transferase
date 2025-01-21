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

#include <command_intervals.hpp>

#include "unit_test_utils.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>  // for EXIT_SUCCESS
#include <filesystem>
#include <string>
#include <system_error>

TEST(command_intervals_test, basic_local_test) {
  // Input files for test
  static constexpr auto index_directory = "data";
  static constexpr auto genome_name = "pAntiquusx";
  static constexpr auto methylome_directory = "data";
  static constexpr auto methylome_name = "SRX012346";
  static constexpr auto intervals_file = "data/pAntiquusx_promoters.bed";
  // Output filename and expected output
  static constexpr auto output_file = "data/output_file.bed";
  static constexpr auto expected_output_file =
    "data/pAntiquusx_promoters_local.bed";

  // Define command line arguments
  const auto argv = std::array{
    // clang-format off
    "intervals",
    "--local",
    "-x",
    index_directory,
    "-g",
    genome_name,
    "-d",
    methylome_directory,
    "-m",
    methylome_name,
    "-i",
    intervals_file,
    "-o",
    output_file,
    // clang-format on
  };
  const int command_argc = sizeof(argv) / sizeof(argv[0]);

  // Run the main function
  // NOLINTBEGIN(cppcoreguidelines-pro-type-const-cast)
  const int result =
    command_intervals_main(command_argc, const_cast<char **>(argv.data()));
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

TEST(command_intervals_test, basic_local_test_scores) {
  // Input files for test
  static constexpr auto index_directory = "data";
  static constexpr auto genome_name = "pAntiquusx";
  static constexpr auto methylome_directory = "data";
  static constexpr auto methylome_name = "SRX012346";
  static constexpr auto intervals_file = "data/pAntiquusx_promoters.bed";
  // Output filename and expected output
  static constexpr auto output_file = "data/output_file.bed";
  static constexpr auto unexpected_output_file =
    "data/pAntiquusx_promoters_local.bed";

  // Define command line arguments
  const auto argv = std::array{
    // clang-format off
    "intervals",
    "--local",
    "-x",
    index_directory,
    "--genome",
    genome_name,
    "-d",
    methylome_directory,
    "-m",
    methylome_name,
    "-i",
    intervals_file,
    "-o",
    output_file,
    "--score",
    // clang-format on
  };
  const int command_argc = sizeof(argv) / sizeof(argv[0]);

  // Run the main function
  // NOLINTBEGIN(cppcoreguidelines-pro-type-const-cast)
  const int result =
    command_intervals_main(command_argc, const_cast<char **>(argv.data()));
  // NOLINTEND(cppcoreguidelines-pro-type-const-cast)

  // Check that the output file is created
  EXPECT_EQ(result, EXIT_SUCCESS);
  EXPECT_TRUE(std::filesystem::exists(output_file));
  EXPECT_FALSE(files_are_identical(output_file, unexpected_output_file));

  // Clean up: delete test files
  std::error_code ignored_ec;
  if (std::filesystem::exists(output_file, ignored_ec)) {
    const auto remove_ok = std::filesystem::remove(output_file, ignored_ec);
    EXPECT_TRUE(remove_ok);
  }
}

TEST(command_intervals_test, failing_remote_test) {
  // Input files for test
  static constexpr auto index_file = "data/pAntiquusx.cpg_idx";
  static constexpr auto accession = "SRX012346";
  static constexpr auto intervals_file = "data/pAntiquusx_promoters.bed";
  // Output filename and expected output
  static constexpr auto output_file = "data/remote_output_file.bed";

  // Define command line arguments
  const auto argv = std::array{
    // clang-format off
    "intervals",
    "-s",
    "localhost",
    "-p",
    "5000",
    "-x",
    index_file,
    "-i",
    intervals_file,
    "-a",
    accession,
    "-o",
    output_file,
    // clang-format on
  };
  const int command_argc = sizeof(argv) / sizeof(argv[0]);

  // Run the main function
  // NOLINTBEGIN(cppcoreguidelines-pro-type-const-cast)
  const int result =
    command_intervals_main(command_argc, const_cast<char **>(argv.data()));
  // NOLINTEND(cppcoreguidelines-pro-type-const-cast)

  // Check that the output file is created
  EXPECT_EQ(result, EXIT_FAILURE);
  EXPECT_FALSE(std::filesystem::exists(output_file));

  // Clean up: delete test files
  if (std::filesystem::exists(output_file)) {
    const auto remove_ok = std::filesystem::remove(output_file);
    EXPECT_TRUE(remove_ok);
  }
}
