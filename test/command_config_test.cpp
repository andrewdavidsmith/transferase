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

#include <gtest/gtest.h>

#include <cstdlib>  // for EXIT_FAILURE, EXIT_SUCCESS
#include <filesystem>
#include <iterator>  // for std::size
#include <string>
#include <vector>

TEST(command_config_test, run_success) {
  std::vector<const char *> argv = {
    "config",
    "-v",
    "critical",
    "-c",
    "config/mxe_client_config.toml",
    "-s",
    "example.com",
    "-p",
    "5000",
  };
  EXPECT_EQ(std::size(argv), 9);
  EXPECT_EQ(argv[0], "config");
  const int ret =
    command_config_main(std::size(argv), const_cast<char **>(argv.data()));
  EXPECT_EQ(ret, EXIT_SUCCESS);

  const std::filesystem::path config_file{"config/mxe_client_config.toml"};
  EXPECT_TRUE(std::filesystem::remove(config_file));
  EXPECT_TRUE(std::filesystem::remove_all(config_file.parent_path()));
}
