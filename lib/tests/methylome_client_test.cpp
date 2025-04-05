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

#include <remote_client.hpp>

#include <logger.hpp>
#include <methylome_name_list.hpp>

#include <client_config.hpp>
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace transferase;  // NOLINT

class remote_client_mock : public ::testing::Test {
protected:
  auto
  SetUp() -> void override {
    static const std::string last_dir_part = "data/lutions";
    logger::instance(shared_from_cout(), "none", log_level_t::debug);
    config_dir = (std::filesystem::current_path() / last_dir_part).string();
  }

  auto
  TearDown() -> void override {}

public:
  std::string hostname{"bulbapedia.bulbagarden.net"};
  std::string port{"9000"};
  std::string index_dir{"indexes"};
  std::string metadata_file{"metadata.json"};

  std::uint32_t n_lutions_available{3};
  std::uint32_t n_lutions_tissues{3};

  std::string config_dir;
  std::string config_file;
};

TEST_F(remote_client_mock, read_failure) {
  static constexpr auto config_dir_mock = ".../asdf";
  EXPECT_THROW(
    { [[maybe_unused]] const auto _ = remote_client(config_dir_mock); },
    std::system_error);
}

TEST_F(remote_client_mock, read_success_throwing) {
  EXPECT_NO_THROW(
    { [[maybe_unused]] const auto client = remote_client(config_dir); });
}

TEST_F(remote_client_mock, read_success) {
  const auto client = remote_client(config_dir);
  EXPECT_EQ(client.config.hostname, hostname)
    << client.config.tostring() << "\n";
  EXPECT_EQ(client.config.port, port);
  EXPECT_FALSE(client.config.index_dir.empty());
}
