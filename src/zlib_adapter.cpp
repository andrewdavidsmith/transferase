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

#include <string>
#include <system_error>

gzinfile::gzinfile(const std::string &filename, std::error_code &ec) {
  in = gzopen(filename.data(), "rb");
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
