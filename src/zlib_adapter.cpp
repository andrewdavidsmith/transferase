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

#include "zlib_adapter.hpp"

#include <zlib.h>

#include <algorithm>  // for std::ranges::copy_n
#include <cerrno>     // for errno
#include <cstdio>     // std::fread
#include <filesystem>
#include <ranges>  // IWYU pragma: keep
#include <string>
#include <system_error>
#include <tuple>
#include <utility>  // std::move
#include <vector>

[[nodiscard]] auto
is_gzip_file(const std::string &filename) -> bool {
  auto f = std::fopen(filename.data(), "rb");
  if (f == nullptr)
    return false;

  // Read the first two bytes of the file
  std::array<std::uint8_t, 2> buf{};
  if (std::fread(buf.data(), 1, 2, f) != 2)
    return false;
  std::fclose(f);

  // Check if the first two bytes match the gzip magic number
  return (buf[0] == 0x1F && buf[1] == 0x8B);
}

gzinfile::gzinfile(const std::string &filename, std::error_code &ec) :
  in{gzopen(filename.data(), "rb")} {
  // ADS: need to check errnum by gzerror
  ec = (in == nullptr) ? zlib_adapter_error::z_errno : zlib_adapter_error::ok;
}

gzinfile::~gzinfile() {
  if (in) {
    gzclose(in);
    in = nullptr;
  }
}

[[nodiscard]] auto
gzinfile::read() -> int {
  len = gzread(in, buf.data(), buf_size);
  // ADS: check errnum from gzerror and use zlib_adapter_error;
  pos = 0;
  return len;
}

[[nodiscard]] auto
gzinfile::getline(std::string &line) -> gzinfile & {
  if (pos == len && !read()) {
    if (in) {
      gzclose(in);
      in = nullptr;
    }
    return *this;
  }
  line.clear();
  while (buf[pos] != '\n') {
    line += buf[pos++];
    if (pos == len && !read()) {
      if (in) {
        gzclose(in);
        in = nullptr;
      }
      return *this;
    }
  }
  ++pos;  // if here, then buf[pos] == '\n'
  return *this;
}

[[nodiscard]]
auto
read_gzfile_into_buffer(const std::string &filename)
  -> std::tuple<std::vector<char>, std::error_code> {
  static constexpr auto buf_size = 1024 * 1024;

  // Open the gzipped file
  gzFile gz = gzopen(filename.data(), "rb");
  if (!gz)
    return {{}, std::make_error_code(std::errc(errno))};

  std::array<char, buf_size> buf;
  std::vector<char> buffer;
  buffer.reserve(std::filesystem::file_size(filename));
  std::int32_t n_bytes{};
  while ((n_bytes = gzread(gz, buf.data(), buf_size)) > 0)
    std::ranges::copy_n(buf.data(), n_bytes, std::back_inserter(buffer));
  gzclose(gz);

  if (n_bytes < 0)
    return {{}, std::error_code{zlib_adapter_error::unexpected_return_code}};

  return {std::move(buffer), {}};  // Success
}
