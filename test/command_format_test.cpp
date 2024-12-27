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

#include "unit_test_utils.hpp"

#include <methylome_data.hpp>
#include <methylome_metadata.hpp>

#include <gtest/gtest.h>

#include <algorithm>  // for std::equal
#include <cstdlib>    // for EXIT_SUCCESS
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

TEST(command_format_test, basic_test) {
  // Input files for test
  static constexpr auto methylome_directory = "data/lutions/methylomes";
  static constexpr auto index_directory = "data/lutions/indexes";
  static constexpr auto output_directory = "data/lutions";
  static constexpr auto genome_name = "eFlareon";
  static constexpr auto methylome_name = "eFlareon_brain";
  static constexpr auto methylation_file =
    "data/lutions/raw/eFlareon_brain.sym.gz";

  // Define command line arguments
  const char *command_argv[] = {
    // clang-format off
    "format",
    "-x",
    index_directory,
    "-g",
    genome_name,
    "-o",
    output_directory,
    "-m",
    methylation_file,
    // clang-format on
  };
  const int command_argc = sizeof(command_argv) / sizeof(command_argv[0]);

  // Run the main function
  const int result =
    command_format_main(command_argc, const_cast<char **>(command_argv));
  EXPECT_EQ(result, EXIT_SUCCESS);

  const auto data_outfile =
    compose_methylome_data_filename(output_directory, methylome_name);
  std::error_code ec;
  const auto data_outfile_exists = std::filesystem::exists(data_outfile, ec);
  EXPECT_EQ(ec, std::error_code{});
  EXPECT_TRUE(data_outfile_exists);

  const auto expected_data_outfile =
    compose_methylome_data_filename(methylome_directory, methylome_name);
  EXPECT_TRUE(files_are_identical(data_outfile, expected_data_outfile));

  if (std::filesystem::exists(data_outfile, ec))
    std::filesystem::remove(data_outfile, ec);
  EXPECT_EQ(ec, std::error_code{});

  const auto meta_outfile =
    compose_methylome_metadata_filename(output_directory, methylome_name);
  if (std::filesystem::exists(meta_outfile, ec))
    std::filesystem::remove(meta_outfile, ec);
  EXPECT_EQ(ec, std::error_code{});
}
