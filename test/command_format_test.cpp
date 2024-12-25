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

#include <command_format.hpp>

// #include <cpg_index_data.hpp>
// #include <cpg_index_metadata.hpp>

#include <gtest/gtest.h>

#include <algorithm>  // for std::equal
#include <cstdlib>    // for EXIT_SUCCESS
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

[[nodiscard]] static inline bool
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

TEST(command_format_test, basic_test) {
  // Input files for test
  static constexpr auto genome_directory = "data/lutions/raw";
  static constexpr auto index_directory = "data/lutions/indexes";
  static constexpr auto output_directory = "data";
  static constexpr auto genome_name = "eFlareon";

  const auto genome_path = std::filesystem::path{genome_directory} /
                           std::format(genome_name, ".fa.gz");

  // Define command line arguments
  const char *command_argv[] = {
    // clang-format off
    "index",
    "-x",
    output_directory,
    "-g",
    "data/lutions/raw/eFlareon.fa.gz", // genome_path.string().data(),
    "-v",
    "debug",
    // clang-format on
  };
  const int command_argc = sizeof(command_argv) / sizeof(command_argv[0]);

  // Run the main function
  const int result =
    command_index_main(command_argc, const_cast<char **>(command_argv));

  const auto data_outfile =
    compose_cpg_index_data_filename(output_directory, genome_name);
  // Check that the output file is created
  EXPECT_EQ(result, EXIT_SUCCESS);
  std::error_code ec;
  const auto data_outfile_exists = std::filesystem::exists(data_outfile, ec);
  EXPECT_EQ(ec, std::error_code{});
  EXPECT_TRUE(data_outfile_exists);

  const auto expected_data_outfile =
    compose_cpg_index_data_filename(index_directory, genome_name);
  EXPECT_TRUE(are_files_identical(data_outfile, expected_data_outfile));

  if (std::filesystem::exists(data_outfile, ec))
    std::filesystem::remove(data_outfile, ec);
  EXPECT_EQ(ec, std::error_code{});

  const auto meta_outfile =
    compose_cpg_index_metadata_filename(output_directory, genome_name);
  if (std::filesystem::exists(meta_outfile, ec))
    std::filesystem::remove(meta_outfile, ec);
  EXPECT_EQ(ec, std::error_code{});
}
