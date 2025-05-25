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

#include <command_query.hpp>

#include "unit_test_utils_cli.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>  // for EXIT_SUCCESS
#include <filesystem>
#include <iterator>  // for std::size
#include <string>
#include <system_error>

TEST(command_query_test, intervals_basic_local_test) {
  // Input files for test
  static constexpr auto index_directory = "data";
  static constexpr auto genome_name = "pAntiquusx";
  static constexpr auto methylome_directory = "data";
  static constexpr auto methylome_name = "SRX012346";
  static constexpr auto intervals_file = "data/pAntiquusx_promoters.bed";
  // Output filename and expected output
  static constexpr auto output_file = "data/output_file.txt";
  static constexpr auto expected_output_file =
    "data/pAntiquusx_promoters_local.txt";

  // Define command line arguments
  const auto argv = std::array{
    // clang-format off
    "query",
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
    "--bed",
    // clang-format on
  };
  const int argc = static_cast<int>(std::size(argv));

  // Run the main function
  // NOLINTNEXTLINE(*-const-cast)
  const int result = command_query_main(argc, const_cast<char **>(argv.data()));

  // Check that the output file is created
  EXPECT_EQ(result, EXIT_SUCCESS);
  std::error_code error;
  EXPECT_TRUE(std::filesystem::exists(output_file, error));

  const bool output_files_identical =
    files_are_identical_cli(output_file, expected_output_file);
  EXPECT_TRUE(output_files_identical);

  // Clean up: delete test files only if tests pass
  if (output_files_identical) {
    remove_file_cli(output_file, error);
    EXPECT_FALSE(error);
  }
}

TEST(command_query_test, intervals_basic_local_test_scores) {
  // Input files for test
  static constexpr auto index_directory = "data";
  static constexpr auto genome_name = "pAntiquusx";
  static constexpr auto methylome_directory = "data";
  static constexpr auto methylome_name = "SRX012346";
  static constexpr auto intervals_file = "data/pAntiquusx_promoters.bed";
  // Output filename and expected output
  static constexpr auto output_file = "data/output_file.txt";
  static constexpr auto unexpected_output_file =
    "data/pAntiquusx_promoters_local.txt";

  // Define command line arguments
  const auto argv = std::array{
    // clang-format off
    "query",
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
    "--scores",
    // clang-format on
  };
  const int argc = static_cast<int>(std::size(argv));

  // Run the main function
  // NOLINTNEXTLINE(*-const-cast)
  const int result = command_query_main(argc, const_cast<char **>(argv.data()));

  // Check that the output file is created
  EXPECT_EQ(result, EXIT_SUCCESS);
  std::error_code error;
  EXPECT_TRUE(std::filesystem::exists(output_file, error));
  const bool output_files_identical =
    files_are_identical_cli(output_file, unexpected_output_file);
  EXPECT_FALSE(output_files_identical);

  // Clean up: delete test files only if tests pass
  if (!output_files_identical) {
    remove_file_cli(output_file, error);
    EXPECT_FALSE(error);
  }
}

TEST(command_query_test, intervals_failing_remote_test) {
  // Input files for test
  static constexpr auto genome_name = "pAntiquusx";
  static constexpr auto index_file = "data/pAntiquusx.cpg_idx";
  static constexpr auto methylome_name = "SRX012346";
  static constexpr auto intervals_file = "data/pAntiquusx_promoters.bed";
  static constexpr auto bad_port = "123";
  // Output filename and expected output
  static constexpr auto output_file = "data/remote_output_file.txt";

  // Define command line arguments
  const auto argv = std::array{
    // clang-format off
    "query",
    "-s",
    "localhost",
    "-p",
    bad_port,
    "-g",
    genome_name,
    "-x",
    index_file,
    "-i",
    intervals_file,
    "-m",
    methylome_name,
    "-o",
    output_file,
    // clang-format on
  };
  const int argc = static_cast<int>(std::size(argv));

  // Run the main function
  // NOLINTNEXTLINE(*-const-cast)
  const int result = command_query_main(argc, const_cast<char **>(argv.data()));

  // Check that the output file is created
  EXPECT_NE(result, EXIT_SUCCESS);
  EXPECT_FALSE(std::filesystem::exists(output_file));

  // Clean up: delete test files
  if (std::filesystem::exists(output_file)) {
    const auto remove_ok = std::filesystem::remove(output_file);
    EXPECT_TRUE(remove_ok);
  }
}

TEST(command_query_test, bins_basic_local_test) {
  // Input files for test
  static constexpr auto index_directory = "data";
  static constexpr auto genome_name = "pAntiquusx";
  static constexpr auto methylome_directory = "data";
  static constexpr auto methylome_name = "SRX012346";
  // Output filename and expected output
  static constexpr auto output_file = "data/output_file.txt";
  static constexpr auto expected_output_file =
    "data/SRX012346_bin100_local.txt";

  // Define command line arguments
  const auto argv = std::array{
    // clang-format off
    "query",
    "--local",
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
    "--bed",
    // clang-format on
  };
  const int argc = static_cast<int>(std::size(argv));

  // Run the main function
  // NOLINTNEXTLINE(*-const-cast)
  const int result = command_query_main(argc, const_cast<char **>(argv.data()));

  // Check that the output file is created
  EXPECT_EQ(result, EXIT_SUCCESS);
  std::error_code error;
  EXPECT_TRUE(std::filesystem::exists(output_file, error));

  const bool output_files_identical =
    files_are_identical_cli(output_file, expected_output_file);
  EXPECT_TRUE(output_files_identical);

  // Clean up: delete test files only if tests pass
  if (output_files_identical) {
    remove_file_cli(output_file, error);
    EXPECT_FALSE(error);
  }
}
