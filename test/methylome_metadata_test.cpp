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

#include <methylome_metadata.hpp>

#include "unit_test_utils.hpp"

#include <config.h>

#include <gtest/gtest.h>

#include <string>
#include <system_error>

using namespace transferase;  // NOLINT

[[nodiscard]] static inline auto
mock_get_username() -> std::string {
  return "test_user";
}

[[nodiscard]] static inline auto
mock_get_genome_name() -> std::string {
  return "mUnicornicus";
}

[[nodiscard]] static inline auto
mock_get_hostname() -> std::string {
  return "test_host";
}

[[nodiscard]] static inline auto
mock_get_time_as_string() -> std::string {
  return "1999-12-31T23:59:59";
}

[[nodiscard]] static inline auto
mock_get_version() -> std::string {
  return "9.9.9";
}

TEST(methylome_metadata_test, is_valid_test) {
  methylome_metadata meta;
  EXPECT_FALSE(meta.is_valid());

  meta.version = mock_get_version();
  EXPECT_FALSE(meta.is_valid());

  meta.host = mock_get_hostname();
  EXPECT_FALSE(meta.is_valid());

  meta.user = mock_get_username();
  EXPECT_FALSE(meta.is_valid());

  meta.creation_time = mock_get_time_as_string();
  EXPECT_FALSE(meta.is_valid());

  meta.genome_name = mock_get_genome_name();
  EXPECT_TRUE(meta.is_valid());
}

TEST(methylome_metadata_test, init_env_test) {
  methylome_metadata meta;
  const std::error_code ec = meta.init_env();
  EXPECT_FALSE(ec);

  meta.genome_name = mock_get_genome_name();
  EXPECT_TRUE(meta.is_valid());
}

TEST(methylome_metadata_test, consistent_test) {
  methylome_metadata meta1;
  methylome_metadata meta2;

  EXPECT_TRUE(meta1.is_consistent(meta2));
  EXPECT_TRUE(meta2.is_consistent(meta1));

  const auto init1_err = meta1.init_env();
  EXPECT_FALSE(init1_err);

  const auto init2_err = meta2.init_env();
  EXPECT_FALSE(init2_err);

  EXPECT_TRUE(meta1.is_consistent(meta2));
  EXPECT_TRUE(meta2.is_consistent(meta1));

  meta2.genome_name = mock_get_genome_name();
  EXPECT_TRUE(meta2.is_valid());
  EXPECT_FALSE(meta2.is_consistent(meta1));
}

TEST(methylome_metadata_test, successful_read) {
  static constexpr auto methylome_directory = "data/lutions/methylomes";
  static constexpr auto methylome_name = "eFlareon_brain";
  const auto json_filename =
    methylome_metadata::compose_filename(methylome_directory, methylome_name);
  std::error_code ec;
  const auto meta = methylome_metadata::read(json_filename, ec);
  EXPECT_FALSE(ec);
  EXPECT_TRUE(meta.is_valid());
}

TEST(methylome_metadata_test, failing_read) {
  static constexpr auto methylome_directory = "data/lutions/methylomes";
  static constexpr auto methylome_name = "eFlareon_brainZZZ";
  const auto json_filename =
    methylome_metadata::compose_filename(methylome_directory, methylome_name);
  std::error_code ec;
  const auto meta = methylome_metadata::read(json_filename, ec);
  EXPECT_TRUE(ec);
  EXPECT_FALSE(meta.is_valid());
}

TEST(methylome_metadata_test, compose_methylome_metadata_filename_test) {
  static constexpr auto methylome_directory = "data/lutions/methylomes";
  static constexpr auto methylome_name = "eFlareon_brain";
  static constexpr auto expected_filename =
    "data/lutions/methylomes/eFlareon_brain.m16.json";
  const auto filename =
    methylome_metadata::compose_filename(methylome_directory, methylome_name);
  EXPECT_EQ(filename, expected_filename);
}

TEST(methylome_metadata_test, write_test) {
  methylome_metadata metadata;
  metadata.host = "test_host";
  metadata.user = "test_user";
  metadata.version = VERSION;
  metadata.creation_time = "2024-12-24T12:34:56";

  const auto outfile =
    generate_temp_filename("output", methylome_metadata::filename_extension);

  std::error_code ec = metadata.write(outfile);
  EXPECT_FALSE(ec);
  const auto outfile_exists = std::filesystem::exists(outfile, ec);
  EXPECT_FALSE(ec);
  EXPECT_TRUE(outfile_exists);

  if (outfile_exists) {
    const bool remove_ok = std::filesystem::remove(outfile, ec);
    EXPECT_TRUE(remove_ok);
  }
  EXPECT_FALSE(ec);
}
