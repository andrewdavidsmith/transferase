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

#include <command_config.hpp>

#include "unit_test_utils_cli.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <filesystem>
#include <iterator>  // for std::size
#include <string>

TEST(command_config_test, run_success) {
  static constexpr auto config_dir = "a_config_dir";
  static constexpr auto argv = std::array{
    // clang-format off
    "config",
    "-c",
    config_dir,
    "-v",
    "critical",
    "-s",
    "example.com",
    "-p",
    "5000",
    "--download",
    "none",
    // clang-format on
  };
  // NOLINT(*-narrowing-conversions)
  const int argc = static_cast<int>(std::size(argv));

  EXPECT_EQ(argv[0], "config");
  // NOLINTNEXTLINE(*-const-cast)
  const int ret = command_config_main(argc, const_cast<char **>(argv.data()));
  EXPECT_EQ(ret, EXIT_SUCCESS);

  std::error_code error;
  remove_directories_cli(config_dir, error);
  EXPECT_FALSE(error);
}
