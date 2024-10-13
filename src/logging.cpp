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

#include "logging.hpp"

#include <unistd.h>

#include <array>
#include <chrono>
#include <cstring>  // std::memcpy
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

using std::array;
using std::format_to;
using std::lock_guard;
using std::memcpy;
using std::string;
using std::string_view;
using std::to_chars;
using std::uint32_t;

namespace chrn = std::chrono;

enum class log_level : uint32_t {
  DEBUG,
  INFO,
  ERROR,
};
static constexpr uint32_t n_levels = 3;
static constexpr array<const char *, n_levels> level_name = {
  "DEBUG",
  "INFO",
  "ERROR",
};
static constexpr array<uint32_t, n_levels> level_name_sz = {
  5,
  4,
  5,
};

static constexpr string_view date_time_fmt_expanded = "YYYY-MM-DD HH:MM:SS";
static constexpr uint32_t date_time_fmt_size = size(date_time_fmt_expanded);
static constexpr auto delim = ' ';
static constexpr auto date_time_fmt = "{:%F}{}{:%T}";

file_logger::file_logger(string log_file_name, string appname) :
  log_file(log_file_name, std::ios::app) {
  buf.fill(' ');
  // ADS TODO: fix this so we actually handle errors
  if (const auto ec = set_attributes(appname)) {
    return;
  }
}

auto
file_logger::format_time() -> void {
  const auto now{chrn::system_clock::now()};
  const chrn::year_month_day ymd{chrn::floor<chrn::days>(now)};
  const chrn::hh_mm_ss hms{chrn::floor<chrn::seconds>(now) -
                           chrn::floor<chrn::days>(now)};
  std::format_to(buf.data(), "{:%F}{}{:%T}", ymd, delim, hms);
}

[[nodiscard]] auto
file_logger::set_attributes(std::string_view appname) -> int {
  // hostname
  static constexpr uint32_t max_hostname_size{256};
  string hostname;
  hostname.resize(max_hostname_size);
  if (gethostname(hostname.data(), max_hostname_size))  // unistd.h
    return errno;
  hostname.resize(hostname.find('\0'));

  // fill the buffer (after the time fields)
  cursor = buf.data() + date_time_fmt_size;
  *cursor++ = delim;

  memcpy(cursor, hostname.data(), size(hostname));
  cursor += size(hostname);
  *cursor++ = delim;

  memcpy(cursor, appname.data(), size(appname));
  cursor += size(appname);
  *cursor++ = delim;

  std::to_chars_result tcr{cursor, std::errc()};
  const auto process_id = getpid();  // unistd.h
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wstringop-overflow=0"
  tcr = to_chars(tcr.ptr, buf_end, process_id);
  *tcr.ptr++ = delim;
#pragma GCC diagnostic push

  cursor = tcr.ptr;

  return 0;
};

// ADS TODO: fix redundant code below
auto
file_logger::debug(string_view message) -> void {
  static constexpr auto idx = std::to_underlying(log_level::DEBUG);
  static constexpr auto sz = level_name_sz[idx];
  static constexpr auto lvl = level_name[idx];
  const auto thread_id = gettid();  // unistd.h
  lock_guard lck{mtx};
  format_time();
  auto [buf_data_end, ec] = to_chars(cursor, buf_end, thread_id);
  *buf_data_end++ = delim;
  memcpy(buf_data_end, lvl, sz);
  buf_data_end += sz;
  *buf_data_end++ = delim;
  buf_data_end = format_to(buf_data_end, "{}\n", message);
  log_file.write(buf.data(), std::distance(buf.data(), buf_data_end));
}

auto
file_logger::info(string_view message) -> void {
  static constexpr auto idx = std::to_underlying(log_level::INFO);
  static constexpr auto sz = level_name_sz[idx];
  static constexpr auto lvl = level_name[idx];
  const auto thread_id = gettid();  // unistd.h
  lock_guard lck{mtx};
  format_time();
  auto [buf_data_end, ec] = to_chars(cursor, buf_end, thread_id);
  *buf_data_end++ = delim;
  memcpy(buf_data_end, lvl, sz);
  buf_data_end += sz;
  *buf_data_end++ = delim;
  buf_data_end = format_to(buf_data_end, "{}\n", message);
  log_file.write(buf.data(), std::distance(buf.data(), buf_data_end));
}

auto
file_logger::error(std::string_view message) -> void {
  static constexpr auto idx = std::to_underlying(log_level::ERROR);
  static constexpr auto sz = level_name_sz[idx];
  static constexpr auto lvl = level_name[idx];
  const auto thread_id = gettid();  // unistd.h
  lock_guard lck{mtx};
  format_time();
  auto [buf_data_end, ec] = to_chars(cursor, buf_end, thread_id);
  *buf_data_end++ = delim;
  memcpy(buf_data_end, lvl, sz);
  buf_data_end += sz;
  *buf_data_end++ = delim;
  buf_data_end = format_to(buf_data_end, "{}\n", message);
  log_file.write(buf.data(), std::distance(buf.data(), buf_data_end));
}
