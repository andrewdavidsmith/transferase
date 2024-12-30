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

#include <chrono>
#include <filesystem>
#include <random>
#include <string>

using namespace xfrase;  // NOLINT

auto
generate_unique_dir_name() -> std::string {
  // Generate a random string based on current time and random numbers
  auto now = std::chrono::system_clock::now().time_since_epoch().count();
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(1000, 9999);

  return "/tmp/test_dir_" + std::to_string(now) + "_" +
         std::to_string(dis(gen));
}

auto
change_permissions_to_read_only(const std::filesystem::path &dir) -> void {
  // clang-format off
  std::filesystem::permissions(dir,
                               std::filesystem::perms::owner_read |
                               std::filesystem::perms::owner_exec |
                               std::filesystem::perms::group_read |
                               std::filesystem::perms::group_exec |
                               std::filesystem::perms::others_read,
                               std::filesystem::perm_options::replace);
  // clang-format on
}

TEST(methylome_test, invalid_accession) {
  auto res = is_valid_accession("invalid.accession");
  EXPECT_FALSE(res);
  res = is_valid_accession("invalid/accession");
  EXPECT_FALSE(res);
}

TEST(methylome_test, valid_accessions) {
  auto res = is_valid_accession("eFlareon_brain");
  EXPECT_TRUE(res);
  res = is_valid_accession("SRX012345");
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
    compose_methylome_metadata_filename(output_directory, methylome_name);
  const auto meta_file_exists = std::filesystem::exists(meta_filename, ec);
  EXPECT_FALSE(ec);
  EXPECT_TRUE(meta_file_exists);
  if (meta_file_exists) {
    std::filesystem::remove(meta_filename, ec);
    EXPECT_FALSE(ec);
  }

  const auto data_filename =
    compose_methylome_data_filename(output_directory, methylome_name);
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

  const auto output_directory = generate_unique_dir_name();
  std::error_code ec;
  std::filesystem::create_directory(output_directory, ec);
  EXPECT_FALSE(ec);
  change_permissions_to_read_only(output_directory);

  const auto meth = methylome::read(methylome_directory, methylome_name, ec);
  EXPECT_FALSE(ec);

  ec = meth.write(output_directory, methylome_name);
  EXPECT_EQ(ec, std::errc::permission_denied);

  const auto meta_filename =
    compose_methylome_metadata_filename(output_directory, methylome_name);
  const auto meta_file_exists = std::filesystem::exists(meta_filename, ec);
  EXPECT_FALSE(ec);
  EXPECT_FALSE(meta_file_exists);
  if (meta_file_exists) {
    std::filesystem::remove(meta_filename, ec);
    EXPECT_FALSE(ec);
  }

  const auto data_filename =
    compose_methylome_data_filename(output_directory, methylome_name);
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
