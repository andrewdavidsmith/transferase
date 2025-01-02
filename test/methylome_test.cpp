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

#include <methylome.hpp>

#include <cpg_index.hpp>
#include <methylome_data.hpp>

#include <gtest/gtest.h>

#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

using namespace xfrase;  // NOLINT

[[nodiscard]] auto
generate_unique_dir_name() -> std::string {
  // Generate a random string based on current time and random numbers
  auto now = std::chrono::system_clock::now().time_since_epoch().count();
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(1000, 9999);

  return "/tmp/test_dir_" + std::to_string(now) + "_" +
         std::to_string(dis(gen));
}

[[nodiscard]] auto
change_permissions_to_no_write(const std::filesystem::path &dir)
  -> std::error_code {
  std::error_code ec;
  // clang-format off
  namespace fs = std::filesystem;
  fs::permissions(dir, (fs::perms::owner_read |
                        fs::perms::owner_exec |
                        fs::perms::group_read |
                        fs::perms::group_exec |
                        fs::perms::others_read),
                  fs::perm_options::replace, ec);
  // clang-format on
  return ec;
}

[[nodiscard]] auto
write_should_fail(const std::filesystem::path &dir,
                  std::error_code &ec) -> bool {
  bool write_failed = false;
  std::filesystem::path file = dir / "test_file.txt";
  std::ofstream test_file(file);
  if (!test_file.is_open()) {
    ec = std::make_error_code(std::errc(errno));
    write_failed = true;
  }
  if (!write_failed) {
    test_file << "Test content";
    if (test_file.fail()) {
      ec = std::make_error_code(std::errc(errno));
      write_failed = true;
    }
    std::error_code local_ec;
    const auto file_exists = std::filesystem::exists(file, local_ec);
    if (file_exists)
      std::filesystem::remove(file, local_ec);
  }
  return write_failed;
}

TEST(methylome_test, invalid_accession) {
  auto res = methylome::is_valid_name("invalid.accession");
  EXPECT_FALSE(res);
  res = methylome::is_valid_name("invalid/accession");
  EXPECT_FALSE(res);
}

TEST(methylome_test, valid_accessions) {
  auto res = methylome::is_valid_name("eFlareon_brain");
  EXPECT_TRUE(res);
  res = methylome::is_valid_name("SRX012345");
  EXPECT_TRUE(res);
}

TEST(methylome_test, valid_read) {
  static constexpr auto methylome_directory = "data/lutions/methylomes";
  static constexpr auto methylome_name = "eJolteon_ear";
  std::error_code ec;
  [[maybe_unused]] const auto meth =
    methylome::read(methylome_directory, methylome_name, ec);
  EXPECT_FALSE(ec);
}

TEST(methylome_test, invalid_read) {
  static constexpr auto methylome_directory = "data/lutions/methylomes";
  static constexpr auto methylome_name = "eVolteon_ear";
  std::error_code ec;
  [[maybe_unused]] const auto meth =
    methylome::read(methylome_directory, methylome_name, ec);
  EXPECT_EQ(ec, std::errc::no_such_file_or_directory);
}

TEST(methylome_test, is_consistent) {
  static constexpr auto methylome_directory = "data/lutions/methylomes";
  static constexpr auto methylome_name = "eFlareon_brain";
  std::error_code ec;
  const auto meth = methylome::read(methylome_directory, methylome_name, ec);
  EXPECT_FALSE(ec);
  EXPECT_TRUE(meth.is_consistent());
}

TEST(methylome_test, valid_write) {
  static constexpr auto methylome_directory = "data/lutions/methylomes";
  static constexpr auto output_directory = "data/lutions";
  static constexpr auto methylome_name = "eFlareon_brain";
  std::error_code ec;
  const auto meth = methylome::read(methylome_directory, methylome_name, ec);
  EXPECT_FALSE(ec);

  ec = meth.write(output_directory, methylome_name);
  EXPECT_FALSE(ec);

  const auto meta_filename =
    methylome_metadata::compose_filename(output_directory, methylome_name);
  const auto meta_file_exists = std::filesystem::exists(meta_filename, ec);
  EXPECT_FALSE(ec);
  EXPECT_TRUE(meta_file_exists);
  if (meta_file_exists) {
    std::filesystem::remove(meta_filename, ec);
    EXPECT_FALSE(ec);
  }

  const auto data_filename =
    methylome_data::compose_filename(output_directory, methylome_name);
  const auto data_file_exists = std::filesystem::exists(data_filename, ec);
  EXPECT_FALSE(ec);
  EXPECT_TRUE(data_file_exists);
  if (data_file_exists) {
    std::filesystem::remove(data_filename, ec);
    EXPECT_FALSE(ec);
  }
}

TEST(methylome_test, invalid_write) {
  static constexpr auto methylome_directory = "data/lutions/methylomes";
  static constexpr auto methylome_name = "eFlareon_brain";

  std::error_code ec;
  const auto output_directory = generate_unique_dir_name();
  std::filesystem::create_directory(output_directory, ec);
  if (ec)
    return;

  ec = change_permissions_to_no_write(output_directory);
  if (ec)
    return;

  const auto proceed_this_test = write_should_fail(output_directory, ec);
  if (ec || !proceed_this_test)
    return;

  const auto meth = methylome::read(methylome_directory, methylome_name, ec);
  EXPECT_FALSE(ec);

  ec = meth.write(output_directory, methylome_name);
  EXPECT_EQ(ec, std::errc::permission_denied);

  const auto meta_filename =
    methylome_metadata::compose_filename(output_directory, methylome_name);
  const auto meta_file_exists = std::filesystem::exists(meta_filename, ec);
  EXPECT_FALSE(ec);

  EXPECT_FALSE(meta_file_exists);
  if (meta_file_exists) {
    std::filesystem::remove(meta_filename, ec);
    EXPECT_FALSE(ec);
  }

  const auto data_filename =
    methylome_data::compose_filename(output_directory, methylome_name);
  const auto data_file_exists = std::filesystem::exists(data_filename, ec);
  EXPECT_FALSE(ec);
  EXPECT_FALSE(data_file_exists);
  if (data_file_exists) {
    std::filesystem::remove(data_filename, ec);
    EXPECT_FALSE(ec);
  }

  std::filesystem::remove_all(output_directory, ec);
  EXPECT_FALSE(ec);
}

TEST(methylome_test, init_metadata) {
  static constexpr auto methdir = "data/lutions/methylomes";
  static constexpr auto indexdir = "data/lutions/indexes";
  static constexpr auto methylome_name = "eVaporeon_tail";
  static constexpr auto genome_name = "eVaporeon";

  std::error_code ec;
  const auto index = cpg_index::read(indexdir, genome_name, ec);
  EXPECT_FALSE(ec);

  const auto meta = methylome_metadata::read(methdir, methylome_name, ec);
  EXPECT_FALSE(ec);

  const auto data = methylome_data::read(methdir, methylome_name, meta, ec);
  EXPECT_FALSE(ec);

  methylome meth{data, meta};

  ec = meth.init_metadata(index);
  EXPECT_FALSE(ec);
  EXPECT_TRUE(meth.is_consistent());
}

TEST(methylome_test, update_metadata) {
  static constexpr auto methdir = "data/lutions/methylomes";
  static constexpr auto methylome_name = "eVaporeon_tail";

  std::error_code ec;
  const auto meta = methylome_metadata::read(methdir, methylome_name, ec);
  EXPECT_FALSE(ec);

  const auto data = methylome_data::read(methdir, methylome_name, meta, ec);
  EXPECT_FALSE(ec);

  methylome meth{data, meta};

  ec = meth.update_metadata();
  EXPECT_FALSE(ec);
  EXPECT_TRUE(meth.is_consistent());
}
