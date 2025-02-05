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
#include <fstream>
#include <iterator>  // for std::size
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

using namespace transferase;  // NOLINT

class client_config_mock : public ::testing::Test {
protected:
  auto
  SetUp() -> void override {
    static const std::string last_dir_part = "transferase";
    logger::instance(shared_from_cout(), "none", log_level_t::debug);
    const std::string payload = R"config(hostname = bulbapedia.bulbagarden.net
port = 9000
# index-dir =
# metadata-file =
# methylome-dir =
# log-file =
# log-level = info
)config";
    config_dir = (std::filesystem::current_path() / last_dir_part).string();
    const bool config_dir_exists = std::filesystem::exists(config_dir);
    if (!config_dir_exists) {
      [[maybe_unused]] const bool make_dirs_ok =
        std::filesystem::create_directories(config_dir);
    }
    std::error_code error;
    config_file = client_config::get_config_file(config_dir, error);
    std::ofstream out(config_file);
    if (!out)
      throw std::runtime_error("failed to open config file to write mock");
    const auto sz = static_cast<std::streamsize>(std::size(payload));
    out.write(payload.data(), sz);
  }

  auto
  TearDown() -> void override {
    std::error_code error;
    const auto exists = std::filesystem::exists(config_dir, error);
    if (exists) {
      const auto is_dir = std::filesystem::is_directory(config_dir, error);
      if (is_dir) {
        [[maybe_unused]]
        const auto remove_ok = std::filesystem::remove_all(config_dir, error);
      }
    }
  }

public:
  std::string hostname{"bulbapedia.bulbagarden.net"};
  std::string port{"9000"};
  std::string config_dir;
  std::string config_file;
};

TEST_F(client_config_mock, read_failure) {
  static constexpr auto config_dir_mock = ".../asdf";
  std::error_code error;
  const client_config cfg = client_config::read(config_dir_mock, error);
  EXPECT_TRUE(error);
}

TEST_F(client_config_mock, read_success) {
  std::error_code error;
  const client_config cfg = client_config::read(config_dir, error);
  EXPECT_FALSE(error);
  EXPECT_EQ(cfg.hostname, hostname);
  EXPECT_EQ(cfg.port, port);
}

TEST_F(client_config_mock, validate_failure) {
  std::error_code error;
  client_config cfg{};
  const bool validate_ok = cfg.validate(error);
  EXPECT_FALSE(validate_ok);
  EXPECT_TRUE(error);
}

TEST_F(client_config_mock, validate_success) {
  client_config cfg{};
  std::error_code error;
  cfg.set_defaults(error);
  EXPECT_FALSE(error);
  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);
}

TEST_F(client_config_mock, get_defaults_success) {
  client_config cfg{};
  std::error_code error;
  cfg.set_defaults(config_dir, error);
  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);

  EXPECT_NE(cfg.index_dir, std::string{});
  EXPECT_NE(cfg.metadata_file, std::string{});

  EXPECT_FALSE(cfg.hostname.empty());
  EXPECT_FALSE(cfg.port.empty());
  EXPECT_TRUE(cfg.methylome_dir.empty());
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
  std::filesystem::permissions(config_dir_mock,
std::filesystem::perms::none, std::filesystem::perm_options::replace,
error); EXPECT_FALSE(error);

  error = cfg.make_directories();
  EXPECT_EQ(error, std::errc::permission_denied);

  // Turn all permissions back on to remove the dir
  std::filesystem::permissions(config_dir_mock, std::filesystem::perms::all,
                               std::filesystem::perm_options::replace, error);
EXPECT_FALSE(error);

  const auto dir_exists = std::filesystem::is_directory(config_dir_mock,
error); EXPECT_FALSE(error); EXPECT_TRUE(dir_exists); if (dir_exists) {
    const bool remove_ok = std::filesystem::remove_all(config_dir_mock,
error); EXPECT_FALSE(error); EXPECT_TRUE(remove_ok);
  }
}
*/

TEST_F(client_config_mock, make_directories_success) {
  static constexpr auto config_dir_mock = "config_dir";
  client_config cfg{};

  std::error_code defaults_err;
  cfg.set_defaults(config_dir_mock, defaults_err);
  EXPECT_FALSE(defaults_err);

  std::error_code error;
  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);

  error = cfg.make_directories(config_dir_mock);
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

TEST_F(client_config_mock, run_no_genomes_success) {
  static constexpr auto config_dir_mock = "config_dir";
  const std::vector<std::string> mock_genomes;
  const download_policy_t mock_download_policy = download_policy_t::none;

  std::error_code error;
  client_config cfg{};

  std::error_code defaults_err;
  cfg.set_defaults(config_dir_mock, defaults_err);
  EXPECT_FALSE(defaults_err);

  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);

  cfg.run(config_dir_mock, mock_genomes, mock_download_policy, error);
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
