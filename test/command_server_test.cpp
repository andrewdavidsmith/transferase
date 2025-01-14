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

#include <command_server.hpp>

#include <gtest/gtest.h>

#include <cstdlib>  // for EXIT_FAILURE
#include <string>

TEST(command_server_test, failing_server_missing_directory) {
  static constexpr auto methylome_directory = "data/lutions/methylomes";
  static constexpr auto index_directory = "data/lutions/indexes_non_existant";
  // Define command line arguments
  const auto argv = std::array{
    // clang-format off
    "server",
    "-v",
    "error",
    "-s",
    "localhost",
    "-x",
    index_directory,
    "-m",
    methylome_directory,
    // clang-format on
  };
  const int command_argc = sizeof(argv) / sizeof(argv[0]);
  const int result =
    command_server_main(command_argc, const_cast<char **>(argv.data()));
  EXPECT_EQ(result, EXIT_FAILURE);
}
