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

#include <zlib_adapter.hpp>

#include <utilities.hpp>

#include <gtest/gtest.h>
#include <zlib.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>

// helper function to create a temporary gzip file
[[nodiscard]]
static auto
create_gzipped_file(const std::string &content) -> std::string {
  const auto filename = generate_temp_filename("test_file", "gz");
  gzFile gz = gzopen(filename.data(), "wb");
  assert(gz != nullptr);
  const std::int64_t content_size = std::size(content);
  [[maybe_unused]] const std::int64_t bytes_written =
    gzwrite(gz, content.data(), content_size);
  assert(bytes_written == content_size);
  gzclose(gz);
  return filename;
}

TEST(zlib_adapter_test, valid_gz_file) {
  // prepare a gzipped file with known content
  const std::string content = "This is a test file!";
  const std::string gzfile = create_gzipped_file(content);

  const auto [buffer, ec] = read_gzfile_into_buffer(gzfile);

  EXPECT_FALSE(ec);
  // decompressed size should match the original content
  EXPECT_EQ(std::size(buffer), std::size(content));
  EXPECT_EQ(std::string(std::cbegin(buffer), std::cend(buffer)), content);

  if (std::filesystem::exists(gzfile))
    std::filesystem::remove(gzfile);
}

TEST(zlib_adapter_test, invalid_file) {
  const std::string non_existent_file =
    generate_temp_filename("non_existent_file", "gz");
  const auto [buffer, ec] = read_gzfile_into_buffer(non_existent_file);

  EXPECT_TRUE(ec);  // Should return an error
  EXPECT_EQ(ec, std::errc::no_such_file_or_directory);
}

TEST(zlib_adapter_test, corrupted_gz_file) {
  // Manually create a corrupted gzipped file
  const auto gzfile = generate_temp_filename("corrupted", "gz");
  auto file = fopen(gzfile.data(), "wb");
  ASSERT_NE(file, nullptr);
  // Write the gzip header: Magic Number (0x1F 0x8B), Compression Method
  // (DEFLATE)
  fputc(0x1F, file);  // Magic byte 1
  fputc(0x8B, file);  // Magic byte 2
  fputc(0x08, file);  // Compression method (DEFLATE)
  fputc(0x00, file);  // Flags (0x00)

  // Write arbitrary timestamp (4 bytes, typically 0x00 for no timestamp)
  fputc(0x00, file);  // Modification time (1st byte)
  fputc(0x00, file);  // Modification time (2nd byte)
  fputc(0x00, file);  // Modification time (3rd byte)
  fputc(0x00, file);  // Modification time (4th byte)

  fputc(0x00, file);  // Extra flags (0x00)
  fputc(0x03, file);  // OS (Unix)

  fputs("Not a valid gzipped content", file);
  fclose(file);

  const auto [buffer, ec] = read_gzfile_into_buffer(gzfile);

  // Should return an error
  EXPECT_TRUE(ec);
  // Protocol error (invalid gzip format)
  EXPECT_EQ(ec, zlib_adapter_error::unexpected_return_code);

  if (std::filesystem::exists(gzfile))
    std::filesystem::remove(gzfile);
}

TEST(zlib_adapter_test, larger_file) {
  const std::string content(1024 * 1024, 'A');  // 1MB file with repeated 'A'
  const std::string gzfile = create_gzipped_file(content);

  const auto [buffer, ec] = read_gzfile_into_buffer(gzfile);

  EXPECT_FALSE(ec);
  // decompressed size should match
  EXPECT_EQ(std::size(buffer), std::size(content));
  EXPECT_EQ(std::string(std::cbegin(buffer), std::cend(buffer)), content);

  if (std::filesystem::exists(gzfile))
    std::filesystem::remove(gzfile);
}

TEST(zlib_adapter_test, small_file) {
  const std::string content = "A";
  const std::string gzfile = create_gzipped_file(content);

  const auto [buffer, ec] = read_gzfile_into_buffer(gzfile);

  EXPECT_FALSE(ec);
  EXPECT_EQ(std::size(buffer), std::size(content));
  // Content should be 'A'
  EXPECT_EQ(std::string(std::cbegin(buffer), std::cend(buffer)), content);

  if (std::filesystem::exists(gzfile))
    std::filesystem::remove(gzfile);
}

TEST(zlib_adapter_test, empty_file) {
  const auto gzfile = generate_temp_filename("empty", "gz");
  const auto file = fopen(gzfile.data(), "wb");
  ASSERT_NE(file, nullptr);
  fclose(file);

  const auto [buffer, ec] = read_gzfile_into_buffer(gzfile);

  EXPECT_FALSE(ec);
  EXPECT_EQ(std::size(buffer), 0);  // buffer should be empty

  if (std::filesystem::exists(gzfile))
    std::filesystem::remove(gzfile);
}
