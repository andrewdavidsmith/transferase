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

#ifndef SRC_LOGGING_HPP_
#define SRC_LOGGING_HPP_

#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>

template <typename t_logger_impl>
concept logger_like = requires(t_logger_impl log) {
  log.debug(std::string_view{});
  log.info(std::string_view{});
  log.error(std::string_view{});
};

template <logger_like t_logger_impl> struct logger : t_logger_impl {};

/*
  - date (YYYY-MM-DD)
  - time (HH:MM:SS)
  - hostname
  - appname
  - process id
  - thread id
*/

// ADS: also need a logger for the terminal
struct file_logger {

  static file_logger &instance(std::string log_file_name, std::string appname) {
    static file_logger fl(log_file_name, appname);
    return fl;
  }  // instance

  file_logger(std::string log_file_name, std::string appname);
  auto debug(std::string_view message) -> void;
  auto info(std::string_view message) -> void;
  auto error(std::string_view message) -> void;

  auto format_time() -> void;
  auto set_attributes(std::string_view) -> int;

  static constexpr std::uint32_t buf_size{1024};  // max log line
  std::array<char, buf_size> buf{};
  char *buf_end{buf.data() + buf_size};
  std::ofstream log_file;
  std::mutex mtx{};
  char *cursor{};

  file_logger(const file_logger &) = delete;
  file_logger &operator=(const file_logger &) = delete;

private:
  file_logger() {}
  ~file_logger() {}
};  // struct file_logger

#endif  // SRC_LOGGING_HPP_
