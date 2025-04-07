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

#include <client_config.hpp>
#include <download_policy.hpp>
#include <logger.hpp>
#include <methylome_name_list.hpp>
#include <remote_data_resource.hpp>

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
    const std::string payload = R"config({
    "config_dir": "",
    "hostname": "bulbapedia.bulbagarden.net",
    "index_dir": "",
    "log_file": "",
    "log_level": "debug",
    "methbase_metadata_dataframe": "",
    "methylome_list": "",
    "select_metadata": "",
    "labels_file": "",
    "methylome_dir": "",
    "port": "9000"
}
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
    remove_directories(config_file, error);
    EXPECT_FALSE(error) << error.message();
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

  EXPECT_FALSE(error) << config_dir << "\n"
                      << config_file << "\n"
                      << cfg.tostring() << "\n";
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
  const auto unique_config_dir = generate_unique_dir_name();
  std::error_code error;
  client_config cfg(unique_config_dir, error);
  EXPECT_FALSE(error);
  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);

  remove_directories(unique_config_dir, error);
  EXPECT_FALSE(error);
}

TEST_F(client_config_mock, make_directories_success) {
  static constexpr auto config_dir_mock = "arbitrary_config_dir";

  std::error_code defaults_err;
  client_config cfg(config_dir_mock, defaults_err);
  EXPECT_FALSE(defaults_err);

  std::error_code error;
  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);

  cfg.config_dir = config_dir_mock;
  cfg.make_directories(error);
  EXPECT_FALSE(error);

  remove_directories(config_dir_mock, error);
  EXPECT_FALSE(error);
}

TEST_F(client_config_mock, get_defaults_success) {
  static constexpr auto config_dir_mock = "config_dir";
  std::error_code error;
  client_config cfg(config_dir_mock, error);
  EXPECT_FALSE(error);
  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);

  EXPECT_NE(cfg.index_dir, std::string{});
  EXPECT_NE(cfg.methbase_metadata_dataframe, std::string{});

  EXPECT_FALSE(cfg.hostname.empty());
  EXPECT_FALSE(cfg.port.empty());
  EXPECT_TRUE(cfg.methylome_dir.empty());

  cfg.config_dir = config_dir_mock;
  cfg.make_directories(error);
  EXPECT_FALSE(error) << error.message() << "\n";

  cfg.save(error);
  EXPECT_FALSE(error) << error.message() << "\n";

  remove_directories(config_dir_mock, error);
  EXPECT_FALSE(error);
}

TEST_F(client_config_mock, run_no_genomes_success) {
  static constexpr auto config_dir_mock = "arbitrary_config_dir";
  const std::vector<std::string> mock_genomes;
  const download_policy_t mock_download_policy = download_policy_t::none;

  std::error_code error;
  client_config cfg(config_dir_mock, error);
  EXPECT_FALSE(error) << config_dir_mock << "\n";

  const bool validate_ok = cfg.validate(error);
  EXPECT_TRUE(validate_ok);
  EXPECT_FALSE(error);

  const std::string sys_config_dir_mock_empty;
  EXPECT_NO_THROW({
    cfg.install(mock_genomes, mock_download_policy, sys_config_dir_mock_empty);
  });

  remove_directories(config_dir_mock, error);
  EXPECT_FALSE(error);
}

TEST_F(client_config_mock, re_read_config_file_success) {
  constexpr auto mock_hostname = "some_hostname.org";
  constexpr auto mock_methylome_dir = "some_dir";
  const auto lutions_config_dir = "data/lutions";
  client_config cfg;
  cfg.config_dir = lutions_config_dir;
  cfg.hostname = mock_hostname;
  cfg.methylome_dir = mock_methylome_dir;
  EXPECT_EQ(cfg.hostname, mock_hostname);
  EXPECT_EQ(cfg.methylome_dir, mock_methylome_dir);
  EXPECT_EQ(cfg.port, "");
  EXPECT_EQ(cfg.index_dir, "");
  EXPECT_EQ(cfg.methbase_metadata_dataframe, "");

  std::error_code error;
  cfg.read_config_file_no_overwrite(error);
  EXPECT_FALSE(error) << error.message() << "\n";

  EXPECT_EQ(cfg.hostname, mock_hostname) << cfg.tostring() << "\n";
  EXPECT_EQ(cfg.methylome_dir, mock_methylome_dir) << cfg.tostring() << "\n";
  EXPECT_EQ(cfg.port, "9000") << cfg.tostring() << "\n";
  EXPECT_EQ(cfg.index_dir, "indexes") << cfg.tostring() << "\n";
  EXPECT_EQ(cfg.methbase_metadata_dataframe, std::string{})
    << cfg.tostring() << "\n";
  EXPECT_EQ(cfg.methylome_list, std::format("methylome_list_{}.json", VERSION))
    << cfg.tostring() << "\n";
}
