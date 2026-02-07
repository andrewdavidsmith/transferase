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

#include <command_check.hpp>

#include <gtest/gtest.h>

#include <array>    // for array
#include <cstdlib>  // for EXIT_SUCCESS
#include <string>
#include <string_view>  // for operator""sv

TEST(command_check_test, basic_test) {
  static constexpr auto methylome_directory = "data/lutions/methylomes";
  static constexpr auto index_directory = "data/lutions/indexes";
  static constexpr auto genome_name = "eJolteon";
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto methylome_names = std::array{
    "eJolteon_brain"sv,
    "eJolteon_tail"sv,
    "eJolteon_ear"sv,
  };

  // Define command line arguments
  const auto argv = std::array{
    "check",
    "-v",
    "error",
    "-x",
    index_directory,
    "-g",
    genome_name,
    "-d",
    methylome_directory,
    "-m",
    std::data(methylome_names[0]),
    std::data(methylome_names[1]),
    std::data(methylome_names[2]),
  };
  const int argc = sizeof(argv) / sizeof(argv[0]);

  // Run the main function
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
  const int result =
    command_check_main(argc, const_cast<char **>(std::data(argv)));
  EXPECT_EQ(result, EXIT_SUCCESS);
}
