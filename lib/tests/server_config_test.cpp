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

#include <server_config.hpp>

#include "unit_test_utils.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

using namespace transferase;  // NOLINT
using std::string_literals::operator""s;

class server_config_mock : public ::testing::Test {
protected:
  auto
  SetUp() -> void override {
    logger::instance(shared_from_cout(), "none", log_level_t::debug);
    const std::string payload = R"config({
    "config_dir": "a_server_config_dir",
    "hostname": "localhost",
    "index_dir": "my_indexes",
    "log_file": "",
    "log_level": "debug",
    "max_intervals": 2000000,
    "max_resident": 4096,
    "methylome_dir": "my_methylomes",
    "min_bin_size": 100,
    "n_threads": 128,
    "pid_file": "",
    "port": "5003"
}
)config";
    std::ofstream out(config_file);
    if (!out)
      throw std::runtime_error("failed to open config file to write mock");
    const auto sz = static_cast<std::streamsize>(std::size(payload));
    out.write(payload.data(), sz);
  }

  auto
  TearDown() -> void override {
    const auto exists = std::filesystem::exists(config_file);
    if (exists)
      std::filesystem::remove(config_file);
  }

  std::string config_file{server_config::server_config_filename_default};
  std::string config_dir{"a_server_config_dir"};
  std::string hostname{"localhost"};
  std::string port{"5003"};
  std::string methylome_dir{"my_methylomes"};
  std::string index_dir{"my_indexes"};
  std::string log_file;
  std::string pid_file;
  log_level_t log_level{log_level_t::debug};
  std::uint32_t n_threads{128};
  std::uint32_t max_resident{4096};
  std::uint32_t min_bin_size{100};
  std::uint32_t max_intervals{2'000'000};
};

TEST(server_config_test, default_constructor_success) {
  EXPECT_NO_THROW(const auto sc = server_config());
}

TEST_F(server_config_mock, get_server_read_fail) {
  EXPECT_THROW(const auto sc = server_config::read("non_existent_file"),
               std::runtime_error);
}

TEST_F(server_config_mock, get_server_read_succeed) {
  server_config sc;
  EXPECT_NO_THROW({ sc = server_config::read(config_file); });
  EXPECT_EQ(sc.hostname, hostname);
  EXPECT_EQ(sc.port, port);
  EXPECT_EQ(sc.methylome_dir, methylome_dir);
  EXPECT_EQ(sc.index_dir, index_dir);
  EXPECT_EQ(sc.log_file, log_file);
  EXPECT_EQ(sc.log_level, log_level);
  EXPECT_EQ(sc.n_threads, n_threads);
  EXPECT_EQ(sc.max_resident, max_resident);
  EXPECT_EQ(sc.min_bin_size, min_bin_size);
  EXPECT_EQ(sc.max_intervals, max_intervals);
}

TEST_F(server_config_mock, validate_success) {
  const auto sc = server_config::read(config_file);
  std::error_code error;
  const bool v = sc.validate(error);
  EXPECT_FALSE(error) << error;
  EXPECT_TRUE(v);
}

TEST_F(server_config_mock, getters_success) {
  const auto sc = server_config::read(config_file);
  EXPECT_EQ(sc.get_index_dir(), "a_server_config_dir/my_indexes")
    << sc.get_index_dir();
  EXPECT_EQ(sc.get_methylome_dir(), "a_server_config_dir/my_methylomes")
    << sc.get_index_dir();
  EXPECT_EQ(sc.get_log_file(), "") << sc.get_index_dir();
}

TEST_F(server_config_mock, read_config_file_no_overwrite) {
  const auto update_hostname = "something_else"s;
  server_config sc;
  sc.hostname = update_hostname;
  std::error_code error;
  sc.read_config_file_no_overwrite(config_file, error);
  EXPECT_FALSE(error);
  EXPECT_EQ(sc.hostname, update_hostname);
  EXPECT_NE(sc.hostname, hostname);
}

TEST_F(server_config_mock, roundtrip_success) {
  const auto update_hostname = "something_else"s;
  server_config sc = server_config::read(config_file);
  sc.hostname = update_hostname;
  const auto tmp_file = generate_temp_filename("tmp");
  sc.write(tmp_file);
  const auto other = server_config::read(tmp_file);
  EXPECT_EQ(sc, other);

  std::error_code error;
  remove_file(tmp_file, error);
  EXPECT_FALSE(error);
}

TEST_F(server_config_mock, make_paths_absolute) {
  server_config sc = server_config::read(config_file);
  sc.make_paths_absolute();
  EXPECT_EQ(sc.index_dir[0], '/');
}

TEST_F(server_config_mock, tostring_success) {
  server_config sc = server_config::read(config_file);
  EXPECT_NO_THROW({ const auto s = sc.tostring(); });
}
