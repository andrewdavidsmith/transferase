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

#include <client_config.hpp>
#include <logger.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

using namespace transferase;  // NOLINT

class client_config_mock : public ::testing::Test {
protected:
  auto
  SetUp() -> void override {
    logger::instance(shared_from_cout(), "none", log_level_t::debug);
  }

  auto
  TearDown() -> void override {}
};

TEST_F(client_config_mock, validate_success) {
  std::error_code error;
  client_config cfg{};
  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);
  EXPECT_NE(cfg.config_dir, std::string{});
  EXPECT_EQ(cfg.hostname, std::string{});
  EXPECT_EQ(cfg.port, std::string{});
}

TEST_F(client_config_mock, get_defaults_success) {
  client_config cfg{};
  std::error_code error = cfg.set_defaults();
  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);

  EXPECT_NE(cfg.config_dir, std::string{});
  EXPECT_NE(cfg.index_dir, std::string{});
  EXPECT_NE(cfg.labels_dir, std::string{});

  EXPECT_EQ(cfg.hostname, std::string{});
  EXPECT_EQ(cfg.port, std::string{});
  EXPECT_EQ(cfg.methylome_dir, std::string{});
}

// ADS: the test below can't work on github runners, so it's out for
// now...
/*
TEST_F(client_config_mock, make_directories_failure) {
  static constexpr auto config_dir_mock = "unwritable";

  std::error_code error;

  client_config cfg{};
  cfg.config_dir = config_dir_mock;
  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);

  const bool mkdir_ok =
    std::filesystem::create_directory(config_dir_mock, error);
  EXPECT_FALSE(error);
  EXPECT_TRUE(mkdir_ok);

  // Turn off all permissions
  std::filesystem::permissions(config_dir_mock, std::filesystem::perms::none,
                               std::filesystem::perm_options::replace, error);
  EXPECT_FALSE(error);

  error = cfg.make_directories();
  EXPECT_EQ(error, std::errc::permission_denied);

  // Turn all permissions back on to remove the dir
  std::filesystem::permissions(config_dir_mock, std::filesystem::perms::all,
                               std::filesystem::perm_options::replace, error);
  EXPECT_FALSE(error);

  const auto dir_exists = std::filesystem::is_directory(config_dir_mock, error);
  EXPECT_FALSE(error);
  EXPECT_TRUE(dir_exists);
  if (dir_exists) {
    const bool remove_ok = std::filesystem::remove_all(config_dir_mock, error);
    EXPECT_FALSE(error);
    EXPECT_TRUE(remove_ok);
  }
}
*/

TEST_F(client_config_mock, make_directories_success) {
  static constexpr auto config_dir_mock = "config_dir";
  std::error_code error;
  client_config cfg{};
  cfg.config_dir = config_dir_mock;
  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);

  error = cfg.make_directories();
  EXPECT_FALSE(error);

  const auto dir_exists = std::filesystem::is_directory(config_dir_mock, error);
  EXPECT_FALSE(error);
  EXPECT_TRUE(dir_exists);
  if (dir_exists) {
    const bool remove_ok = std::filesystem::remove_all(config_dir_mock, error);
    EXPECT_FALSE(error);
    EXPECT_TRUE(remove_ok);
  }
}

TEST_F(client_config_mock, write_failure) {
  static constexpr auto config_dir_mock = "config_dir";
  static constexpr auto bad_config_filename = ".../asdf";
  std::error_code error;
  client_config cfg{};
  cfg.config_dir = config_dir_mock;
  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);

  error = cfg.make_directories();
  EXPECT_FALSE(error);

  cfg.config_file = bad_config_filename;
  error = cfg.write();
  EXPECT_EQ(error, std::errc::no_such_file_or_directory);

  const auto dir_exists = std::filesystem::is_directory(config_dir_mock, error);
  EXPECT_FALSE(error);
  EXPECT_TRUE(dir_exists);
  if (dir_exists) {
    const bool remove_ok = std::filesystem::remove_all(config_dir_mock, error);
    EXPECT_FALSE(error);
    EXPECT_TRUE(remove_ok);
  }
}

TEST_F(client_config_mock, write_success) {
  static constexpr auto config_dir_mock = "config_dir";
  static constexpr auto good_config_filename = "seems_ok";
  std::error_code error;
  client_config cfg{};
  cfg.config_dir = config_dir_mock;
  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);

  error = cfg.make_directories();
  EXPECT_FALSE(error);

  cfg.config_file = good_config_filename;
  error = cfg.write();
  EXPECT_FALSE(error);

  const auto dir_exists = std::filesystem::is_directory(config_dir_mock, error);
  EXPECT_FALSE(error);
  EXPECT_TRUE(dir_exists);
  if (dir_exists) {
    const bool remove_ok = std::filesystem::remove_all(config_dir_mock, error);
    EXPECT_FALSE(error);
    EXPECT_TRUE(remove_ok);
  }
}

TEST_F(client_config_mock, run_bad_sysconfig_failure) {
  static constexpr auto config_dir_mock = "config_dir";
  static constexpr auto bad_sys_config_dir_mock = "/dev";
  const std::vector<std::string> mock_genomes = {"hg38"};
  const bool mock_force_download = false;

  std::error_code error;
  client_config cfg{};
  cfg.config_dir = config_dir_mock;
  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);

  cfg.run(mock_genomes, bad_sys_config_dir_mock, mock_force_download, error);
  EXPECT_EQ(error,
            client_config_error_code::error_identifying_remote_resources);

  const auto dir_exists = std::filesystem::is_directory(config_dir_mock, error);
  EXPECT_FALSE(error);
  EXPECT_TRUE(dir_exists);
  if (dir_exists) {
    const bool remove_ok = std::filesystem::remove_all(config_dir_mock, error);
    EXPECT_FALSE(error);
    EXPECT_TRUE(remove_ok);
  }
}

TEST_F(client_config_mock, run_no_genomes_success) {
  static constexpr auto config_dir_mock = "config_dir";
  const std::vector<std::string> mock_genomes;
  const bool mock_force_download = false;

  std::error_code error;
  client_config cfg{};
  cfg.config_dir = config_dir_mock;
  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);

  cfg.run(mock_genomes, mock_force_download, error);
  EXPECT_FALSE(error) << error.message();

  const auto dir_exists = std::filesystem::is_directory(config_dir_mock, error);
  EXPECT_FALSE(error);
  EXPECT_TRUE(dir_exists);
  if (dir_exists) {
    const bool remove_ok = std::filesystem::remove_all(config_dir_mock, error);
    EXPECT_FALSE(error);
    EXPECT_TRUE(remove_ok);
  }
}
