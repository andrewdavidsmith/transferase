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

#ifndef SRC_LOGGER_HPP_
#define SRC_LOGGER_HPP_

#include "format_error_code.hpp"  // IWYU pragma: keep

#include <boost/describe.hpp>  // for BOOST_DESCRIBE_ENUM

#if not defined(__APPLE__) && not defined(__MACH__)
#include <sys/syscall.h>
#include <unistd.h>  // gettid
#ifndef SYS_gettid
#error "SYS_gettid unavailable on this system"
#endif
#define gettid() ((pid_t)syscall(SYS_gettid))
#else
#include <pthread.h>  // pthread_threadid_np
#endif

#include <sys/types.h>  // for pid_t

#include <array>  // for array
#include <cassert>
#include <charconv>  // for to_chars, to_chars_result
#include <chrono>
#include <cstdint>  // std::uint32_t
#include <cstring>  // std::memcpy
#include <format>
#include <iostream>
#include <iterator>  // for size, distance
#include <memory>
#include <mutex>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>  // std::to_underlying

namespace transferase {
/*
  - date (YYYY-MM-DD)
  - time (HH:MM:SS)
  - hostname
  - appname
  - process id
  - thread id
  - log level
  - message
*/

enum class log_level_t : std::uint8_t {
  debug,
  info,
  warning,
  error,
  critical,
  n_levels,
};
static constexpr std::uint32_t n_log_levels =
  std::to_underlying(log_level_t::n_levels);

// clang-format off
BOOST_DESCRIBE_ENUM(
  log_level_t,
  debug,
  info,
  warning,
  error,
  critical,
  n_levels
)
// clang-format on

static constexpr std::array<const char *, n_log_levels> level_name = {
  // clang-format off
  "debug",
  "info",
  "warning",
  "error",
  "critical",
  // clang-format on
};

static constexpr auto level_name_sz{[]() constexpr {
  std::array<std::uint32_t, n_log_levels> tmp{};
  for (std::uint32_t i = 0; i < n_log_levels; ++i)
    tmp[i] = std::size(std::string_view(level_name[i]));
  return tmp;
}()};

inline auto
operator<<(std::ostream &o, const log_level_t &l) -> std::ostream & {
  return o << level_name[std::to_underlying(l)];
}

inline auto
operator>>(std::istream &in, log_level_t &l) -> std::istream & {
  std::string tmp;
  if (!(in >> tmp))
    return in;
  for (std::uint32_t i = 0; i < n_log_levels; ++i)
    if (tmp == std::string_view(level_name[i])) {
      l = static_cast<log_level_t>(i);
      return in;
    }
  in.setstate(std::ios::failbit);
  return in;
}

[[nodiscard]] inline auto
shared_from_cout() -> std::shared_ptr<std::ostream> {
  return std::make_shared<std::ostream>(std::cout.rdbuf());
}

class logger {
private:
  static constexpr std::string_view date_time_fmt_expanded =
    "YYYY-MM-DD HH:MM:SS";
  static constexpr std::uint32_t date_time_fmt_size =
    std::size(date_time_fmt_expanded);
  static constexpr auto delim = ' ';
  static constexpr auto date_time_fmt = "{:%F}{}{:%T}";
  static constexpr std::array<const char *, n_log_levels> level_name = {
    // clang-format off
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "CRITICAL",
    // clang-format on
  };
  static constexpr auto level_name_sz{[]() constexpr {
    std::array<std::uint32_t, n_log_levels> tmp{};
    for (std::uint32_t i = 0; i < n_log_levels; ++i)
      tmp[i] = std::size(std::string_view(level_name[i]));
    return tmp;
  }()};

public:
  static constexpr log_level_t default_level{log_level_t::info};

  static logger &
  instance(const std::shared_ptr<std::ostream> &log_file_ptr = nullptr,
           const std::string &appname = "",
           log_level_t min_log_level = log_level_t::debug) {
    static logger fl(log_file_ptr, appname, min_log_level);
    assert(fl.log_file != nullptr || log_file_ptr != nullptr);
    return fl;
  }

  logger(const std::shared_ptr<std::ostream> &log_file,
         const std::string &appname,
         log_level_t min_log_level = log_level_t::info);

  [[nodiscard]] auto
  get_status() const -> std::error_code {
    return status;
  }

  operator bool() const { return status ? false : true; }

  template <log_level_t the_level>
  auto
  log(const std::string_view message) -> void {
    static constexpr auto idx = std::to_underlying(the_level);
    static constexpr auto sz = level_name_sz[idx];
    static constexpr auto lvl = level_name[idx];
    if (the_level >= min_log_level) {
      std::uint64_t thread_id{};
#if not defined(__APPLE__) && not defined(__MACH__)
      thread_id = gettid();
#else
      pthread_threadid_np(nullptr, &thread_id);
#endif
      std::lock_guard lck{mtx};
      format_time();
      const auto buf_data_end = fill_buffer(thread_id, lvl, sz, message);
      log_file->write(buf.data(), std::distance(buf.data(), buf_data_end));
      log_file->flush();
    }
  }

  template <log_level_t the_level, typename... Args>
  auto
  log(const std::string_view fmt_str, Args &&...args) -> void {
    static constexpr auto idx = std::to_underlying(the_level);
    static constexpr auto sz = level_name_sz[idx];
    static constexpr auto lvl = level_name[idx];
    if (the_level >= min_log_level) {
      std::uint64_t thread_id{};
#if not defined(__APPLE__) && not defined(__MACH__)
      thread_id = gettid();
#else
      pthread_threadid_np(nullptr, &thread_id);
#endif
      const auto msg = std::vformat(fmt_str, std::make_format_args(args...));
      std::lock_guard lck{mtx};
      format_time();
      const auto buf_data_end = fill_buffer(thread_id, lvl, sz, msg);
      log_file->write(buf.data(), std::distance(buf.data(), buf_data_end));
      log_file->flush();
    }
  }

  auto
  debug(const std::string_view message) -> void {
    log<log_level_t::debug>(message);
  }
  auto
  info(const std::string_view message) -> void {
    log<log_level_t::info>(message);
  }
  auto
  warning(const std::string_view message) -> void {
    log<log_level_t::warning>(message);
  }
  auto
  error(const std::string_view message) -> void {
    log<log_level_t::error>(message);
  }
  auto
  critical(const std::string_view message) -> void {
    log<log_level_t::critical>(message);
  }

  template <typename... Args>
  auto
  debug(const std::string_view fmt_str, Args &&...args) -> void {
    log<log_level_t::debug>(fmt_str, args...);
  }
  template <typename... Args>
  auto
  info(const std::string_view fmt_str, Args &&...args) -> void {
    log<log_level_t::info>(fmt_str, args...);
  }
  template <typename... Args>
  auto
  warning(const std::string_view fmt_str, Args &&...args) -> void {
    log<log_level_t::warning>(fmt_str, args...);
  }
  template <typename... Args>
  auto
  error(const std::string_view fmt_str, Args &&...args) -> void {
    log<log_level_t::error>(fmt_str, args...);
  }
  template <typename... Args>
  auto
  critical(const std::string_view fmt_str, Args &&...args) -> void {
    log<log_level_t::critical>(fmt_str, args...);
  }

private:
  auto
  set_attributes(const std::string_view) -> std::error_code;

  static constexpr std::uint32_t buf_size{1024};  // max log line
  std::array<char, buf_size> buf{};
  char *buf_end{buf.data() + buf_size};
  std::shared_ptr<std::ostream> log_file{nullptr};
  std::mutex mtx{};
  char *cursor{};
  const log_level_t min_log_level{};
  std::error_code status{};

  logger(const logger &) = delete;
  logger &
  operator=(const logger &) = delete;

  logger() {}
  ~logger() {}

  [[nodiscard]] auto
  fill_buffer(std::uint32_t thread_id, const char *const lvl,
              const std::uint32_t sz,
              const std::string_view message) -> char * {
    auto [buf_data_end, ec] = std::to_chars(cursor, buf_end, thread_id);
    *buf_data_end++ = delim;
    std::memcpy(buf_data_end, lvl, sz);
    buf_data_end += sz;
    *buf_data_end++ = delim;
    return std::format_to(buf_data_end, "{}\n", message);
  }

  auto
  format_time() -> void {
    const auto now{std::chrono::system_clock::now()};
    const std::chrono::year_month_day ymd{
      std::chrono::floor<std::chrono::days>(now)};
    const std::chrono::hh_mm_ss hms{
      std::chrono::floor<std::chrono::seconds>(now) -
      std::chrono::floor<std::chrono::days>(now)};
    std::format_to(buf.data(), "{:%F}{}{:%T}", ymd, delim, hms);
  }
};  // struct logger

template <log_level_t lvl, typename... Args>
auto
log_args(std::ranges::input_range auto &&key_value_pairs) {
  logger &lgr = logger::instance();
  for (auto &&[k, v] : key_value_pairs)
    lgr.log<lvl>("{}: {}", k, v);
}

}  // namespace transferase

template <>
struct std::formatter<transferase::log_level_t> : std::formatter<std::string> {
  auto
  format(const transferase::log_level_t &l, std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}",
                          transferase::level_name[std::to_underlying(l)]);
  }
};

#endif  // SRC_LOGGER_HPP_
