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

#include "logger.hpp"

#include <unistd.h>

#include <cstring>  // std::memcpy
#include <string>
#include <string_view>
#include <system_error>

namespace chrn = std::chrono;

[[nodiscard]] auto
logger::set_attributes(std::string_view appname) -> std::error_code {
  // get the hostname
  static constexpr std::uint32_t max_hostname_size{256};
  std::string hostname;
  hostname.resize(max_hostname_size);
  if (gethostname(hostname.data(), max_hostname_size))  // unistd.h
    return std::make_error_code(std::errc(errno));
  hostname.resize(hostname.find('\0'));

  // ADS TODO: error conditions on buffer sizes

  // fill the buffer (after the time fields)
  cursor = buf.data() + date_time_fmt_size;
  *cursor++ = delim;

  // hostname in buffer
  std::memcpy(cursor, hostname.data(), size(hostname));
  cursor += size(hostname);
  *cursor++ = delim;

  // appname in buffer
  std::memcpy(cursor, appname.data(), size(appname));
  cursor += size(appname);
  *cursor++ = delim;

  // process id in buffer
  std::to_chars_result tcr{cursor, std::errc()};
  const auto process_id = getpid();  // syscall;unistd.h
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wstringop-overflow=0"
  tcr = std::to_chars(tcr.ptr, buf_end, process_id);
  *tcr.ptr++ = delim;
#pragma GCC diagnostic push

  if (tcr.ec != std::errc{})
    return std::make_error_code(tcr.ec);

  cursor = tcr.ptr;

  return std::make_error_code(std::errc{});
};
