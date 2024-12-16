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

#include <gtest/gtest.h>

#include <algorithm>  // for std::equal
#include <cstdlib>    // for EXIT_SUCCESS
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

[[nodiscard]] static bool
are_files_identical(const std::filesystem::path &file1,
                    const std::filesystem::path &file2) {
  namespace fs = std::filesystem;
  if (fs::file_size(file1) != fs::file_size(file2))
    return false;

  std::ifstream f1(file1, std::ifstream::binary);
  std::ifstream f2(file2, std::ifstream::binary);
  return std::equal(std::istreambuf_iterator<char>(f1.rdbuf()),
                    std::istreambuf_iterator<char>(),
                    std::istreambuf_iterator<char>(f2.rdbuf()));
}

TEST(CommandIntervalsMainTest, GeneratesAndDeletesFiles) {
  // Input files for test
  static constexpr auto index_file = "data/pAntiquusx.cpg_idx";
  static constexpr auto meth_file = "data/SRX012346.m16";
  static constexpr auto intervals_file = "data/pAntiquusx_promoters.bed";
  // Output filename and expected outptu
  static constexpr auto output_file = "data/output_file.bed";
  static constexpr auto expected_output_file =
    "data/pAntiquusx_promoters_local.bed";

  // Define command line arguments
  const char *command_argv[] = {
    // clang-format off
    "intervals",
    "local",
    "-x",
    index_file,
    "-i",
    intervals_file,
    "-m",
    meth_file,
    "-o",
    output_file,
    // clang-format on
  };
  const int command_argc = sizeof(command_argv) / sizeof(command_argv[0]);

  // Run the main function
  const int result =
    command_intervals_main(command_argc, const_cast<char **>(command_argv));

  // Check that the output file is created
  EXPECT_EQ(result, EXIT_SUCCESS);
  EXPECT_TRUE(std::filesystem::exists(output_file));
  EXPECT_TRUE(are_files_identical(output_file, expected_output_file));

  // Clean up: delete test files
  std::filesystem::remove(output_file);
}
