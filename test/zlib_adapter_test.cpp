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

#include "unit_test_utils.hpp"

#include <gtest/gtest.h>
#include <zlib.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>  // for std::unique_ptr
#include <string>
#include <system_error>

using namespace transferase;  // NOLINT

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

  if (std::filesystem::exists(gzfile)) {
    const bool remove_ok = std::filesystem::remove(gzfile);
    EXPECT_TRUE(remove_ok);
  }
}

TEST(zlib_adapter_test, invalid_file) {
  const std::string non_existent_file =
    generate_temp_filename("non_existent_file", "gz");
  const auto [buffer, ec] = read_gzfile_into_buffer(non_existent_file);

  EXPECT_TRUE(ec);  // Should return an error
  EXPECT_EQ(ec, std::errc::no_such_file_or_directory);
}

static auto
fputc_wrapper(const auto val, auto file) {
  [[maybe_unused]] const auto fputc_code = std::fputc(val, file);
  assert(fputc_code == val);
}

TEST(zlib_adapter_test, corrupted_gz_file) {
  const auto closer = [](FILE *fp) {
    const auto r = std::fclose(fp);  // NOLINT(cppcoreguidelines-owning-memory)
    EXPECT_EQ(r, 0);
  };

  // Manually create a corrupted gzipped file
  const auto gzfile = generate_temp_filename("corrupted", "gz");
  std::unique_ptr<FILE, decltype(closer)> file(std::fopen(gzfile.data(), "wb"),
                                               closer);
  EXPECT_NE(file, nullptr);  // NOLINT
  // Write the gzip header: Magic Number (0x1F 0x8B), Compression Method
  // (DEFLATE)
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
  fputc_wrapper(0x1F, file);  // Magic byte 1
  fputc_wrapper(0x8B, file);  // Magic byte 2
  fputc_wrapper(0x08, file);  // Compression method (DEFLATE)
  fputc_wrapper(0x00, file);  // Flags (0x00)

  // Write arbitrary timestamp (4 bytes, typically 0x00 for no timestamp)
  fputc_wrapper(0x00, file);  // Modification time (1st byte)
  fputc_wrapper(0x00, file);  // Modification time (2nd byte)
  fputc_wrapper(0x00, file);  // Modification time (3rd byte)
  fputc_wrapper(0x00, file);  // Modification time (4th byte)

  fputc_wrapper(0x00, file);  // Extra flags (0x00)
  fputc_wrapper(0x03, file);  // OS (Unix)
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

  fputs("Not a valid gzipped content", file);
  const auto close_code = std::fclose(file);
  EXPECT_EQ(close_code, 0);

  const auto [buffer, ec] = read_gzfile_into_buffer(gzfile);

  // Should return an error
  EXPECT_TRUE(ec);
  // Protocol error (invalid gzip format)
  EXPECT_EQ(ec, zlib_adapter_error_code::unexpected_return_code);

  if (std::filesystem::exists(gzfile)) {
    const bool remove_ok = std::filesystem::remove(gzfile);
    EXPECT_TRUE(remove_ok);
  }
}

TEST(zlib_adapter_test, larger_file) {
  const std::string content(1024 * 1024, 'A');  // 1MB file with repeated 'A'
  const std::string gzfile = create_gzipped_file(content);

  const auto [buffer, ec] = read_gzfile_into_buffer(gzfile);

  EXPECT_FALSE(ec);
  // decompressed size should match
  EXPECT_EQ(std::size(buffer), std::size(content));
  EXPECT_EQ(std::string(std::cbegin(buffer), std::cend(buffer)), content);

  if (std::filesystem::exists(gzfile)) {
    const bool remove_ok = std::filesystem::remove(gzfile);
    EXPECT_TRUE(remove_ok);
  }
}

TEST(zlib_adapter_test, small_file) {
  const std::string content = "A";
  const std::string gzfile = create_gzipped_file(content);

  const auto [buffer, ec] = read_gzfile_into_buffer(gzfile);

  EXPECT_FALSE(ec);
  EXPECT_EQ(std::size(buffer), std::size(content));
  // Content should be 'A'
  EXPECT_EQ(std::string(std::cbegin(buffer), std::cend(buffer)), content);

  if (std::filesystem::exists(gzfile)) {
    const bool remove_ok = std::filesystem::remove(gzfile);
    EXPECT_TRUE(remove_ok);
  }
}

TEST(zlib_adapter_test, empty_file) {
  const auto closer = [](FILE *fp) {
    const auto r = std::fclose(fp);  // NOLINT(cppcoreguidelines-owning-memory)
    EXPECT_EQ(r, 0);
  };

  const auto gzfile = generate_temp_filename("empty", "gz");

  {
    std::unique_ptr<FILE, decltype(closer)> file(
      std::fopen(gzfile.data(), "wb"), closer);
    EXPECT_NE(file, nullptr);  // NOLINT
  }

  const auto [buffer, ec] = read_gzfile_into_buffer(gzfile);

  EXPECT_FALSE(ec);
  EXPECT_EQ(std::size(buffer), 0);  // buffer should be empty

  if (std::filesystem::exists(gzfile)) {
    const bool remove_ok = std::filesystem::remove(gzfile);
    EXPECT_TRUE(remove_ok);
  }
}
