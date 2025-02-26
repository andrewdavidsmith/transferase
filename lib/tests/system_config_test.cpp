/* MIT License
 *
 * Copyright (c) 2025 Andrew D Smith
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

#include "unit_test_utils.hpp"

#include <system_config.hpp>

#include <remote_data_resource.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace transferase;  // NOLINT

TEST(system_config_test, test_system_config_dir_ok) {
  // Essentially tests that the data directory has been created in the test
  // setup
  const auto sys_conf_dir = get_default_system_config_dirname();
  EXPECT_FALSE(sys_conf_dir.empty());
  const auto dir_exists = std::filesystem::exists(sys_conf_dir);
  EXPECT_TRUE(dir_exists) << sys_conf_dir;
  if (dir_exists) {
    const auto dir_is_dir = std::filesystem::is_directory(sys_conf_dir);
    EXPECT_TRUE(dir_is_dir) << sys_conf_dir;
  }
}

TEST(system_config_test, test_system_config_file_ok) {
  // Tests that the data directory contains the expected system config file
  const auto sys_conf_dir = get_default_system_config_dirname();
  EXPECT_FALSE(sys_conf_dir.empty());
  const auto dir_exists = std::filesystem::exists(sys_conf_dir);
  EXPECT_TRUE(dir_exists) << sys_conf_dir;
  if (dir_exists) {
    const auto dir_is_dir = std::filesystem::is_directory(sys_conf_dir);
    EXPECT_TRUE(dir_is_dir) << sys_conf_dir;
    if (dir_is_dir) {
      const auto sys_conf_file = get_system_config_filename();
      const auto sys_conf_path =
        (std::filesystem::path{sys_conf_dir} / sys_conf_file).string();
      const auto file_exists = std::filesystem::exists(sys_conf_path);
      EXPECT_TRUE(file_exists)
        << sys_conf_dir << ", " << sys_conf_file << ", " << sys_conf_path;
    }
  }
}

TEST(system_config_test, test_default_constructor_success) {
  system_config sc;
  EXPECT_EQ(sc.hostname, std::string{});
  EXPECT_EQ(sc.port, std::string{});
  EXPECT_EQ(sc.resources, std::vector<remote_data_resource>{});
}

TEST(system_config_test, test_constructor_from_file_failure) {
  EXPECT_THROW(system_config("some_nonexistent_dir"), std::runtime_error);
}

TEST(system_config_test, test_constructor_from_file_success) {
  const auto dir = get_default_system_config_dirname();
  EXPECT_NO_THROW({ system_config sc(dir); });
}
