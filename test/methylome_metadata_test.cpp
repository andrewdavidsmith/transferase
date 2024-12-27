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

#include <cpg_index.hpp>
#include <methylome_data.hpp>
#include <utilities.hpp>

#include <config.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <system_error>
#include <utility>

// Mock functions to simulate external dependencies
[[nodiscard]]
static inline std::string
mock_get_username() {
  return "test_user";
}

[[nodiscard]]
static inline std::string
mock_get_hostname() {
  return "test_host";
}

[[nodiscard]]
static inline std::string
mock_get_time_as_string() {
  return "2024-12-24T12:34:56";
}

TEST(methylome_metadata_test, init_env_test) {
  methylome_metadata meta;
  const std::error_code ec = meta.init_env();
  EXPECT_FALSE(ec);

  meta.user = mock_get_username();
  meta.creation_time = mock_get_time_as_string();
  meta.host = mock_get_hostname();

  EXPECT_EQ(meta.host, "test_host");
  EXPECT_EQ(meta.user, "test_user");
  EXPECT_EQ(meta.version, VERSION);
  EXPECT_EQ(meta.creation_time, "2024-12-24T12:34:56");
}

TEST(methylome_metadata_test, consistent_test) {
  methylome_metadata meta1;
  methylome_metadata meta2;

  EXPECT_TRUE(meta1.consistent(meta2));
  EXPECT_EQ(meta1.consistent(meta2), meta2.consistent(meta1));

  const auto init1_err = meta1.init_env();
  const auto init2_err = meta2.init_env();

  EXPECT_FALSE(init1_err);
  EXPECT_FALSE(init2_err);
  EXPECT_TRUE(meta1.consistent(meta2));
}

TEST(methylome_metadata_test, compose_methylome_metadata_filename_test) {
  static constexpr auto methylome_directory = "data/lutions/methylomes";
  static constexpr auto methylome_name = "eFlareon_brain";
  static constexpr auto expected_filename =
    "data/lutions/methylomes/eFlareon_brain.m16.json";
  const auto filename =
    compose_methylome_metadata_filename(methylome_directory, methylome_name);
  EXPECT_EQ(filename, expected_filename);
}

TEST(methylome_metadata_test, methylome_metadata_read) {
  static constexpr auto methylome_directory = "data/lutions/methylomes";
  static constexpr auto methylome_name = "eFlareon_brain";
  const auto json_filename =
    compose_methylome_metadata_filename(methylome_directory, methylome_name);
  std::error_code ec;
  [[maybe_unused]] const auto meta =
    methylome_metadata::read(json_filename, ec);
  EXPECT_FALSE(ec);
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

  if (outfile_exists)
    std::filesystem::remove(outfile, ec);
  EXPECT_FALSE(ec);
}

// // Test read method
// TEST(methylome_metadata_test, read) {
//   std::error_code ec;
//   auto metadata = methylome_metadata::read("test.json", ec);
//   EXPECT_FALSE(ec);
//   EXPECT_EQ(metadata.host, "test_host");
//   EXPECT_EQ(metadata.user, "test_user");
//   EXPECT_EQ(metadata.version, VERSION);
//   EXPECT_EQ(metadata.creation_time, "2024-12-24T12:34:56");
// }

// // Test tostring method
// TEST(methylome_metadata_test, tostring) {
//   methylome_metadata metadata;
//   metadata.host = "test_host";
//   metadata.user = "test_user";
//   metadata.version = VERSION;
//   metadata.creation_time = "2024-12-24T12:34:56";

//   std::string result = metadata.tostring();
//   EXPECT_FALSE(result.empty());
// }

// int
// main(int argc, char **argv) {
//   ::testing::InitGoogleTest(&argc, argv);
//   return RUN_ALL_TESTS();
// }
