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

#ifndef LIB_INCLUDE_LOGGER_HPP_
#define LIB_INCLUDE_LOGGER_HPP_

#include "format_error_code.hpp"  // IWYU pragma: keep

#include <boost/describe.hpp>  // for BOOST_DESCRIBE_ENUM

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>  // std::uint32_t
#include <cstring>  // std::memcpy
#include <ctime>    // for localtime_r, strftime, tm, std::strftime
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
  - log level
  - message
*/

using std::literals::string_view_literals::operator""sv;

enum class log_level_t : std::uint8_t {
  debug,
  info,
  warning,
  error,
  critical,
};

// clang-format off
BOOST_DESCRIBE_ENUM(
  log_level_t,
  debug,
  info,
  warning,
  error,
  critical
)
// clang-format on

static constexpr auto level_name = std::array{
  // clang-format off
  "debug"sv,
  "info"sv,
  "warning"sv,
  "error"sv,
  "critical"sv,
  // clang-format on
};

[[nodiscard]] constexpr auto
to_name(const log_level_t l) -> const std::string_view {
  return level_name[std::to_underlying(l)];
}

inline auto
operator<<(std::ostream &o, const log_level_t &l) -> std::ostream & {
  return o << to_name(l);
}

inline auto
operator>>(std::istream &in, log_level_t &l) -> std::istream & {
  std::string tmp;
  if (!(in >> tmp))
    return in;
  for (const auto [idx, name] : std::views::enumerate(level_name))
    if (tmp == name) {
      l = static_cast<log_level_t>(idx);
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
  // ADS: note the extra space at the end of the format string below
  static constexpr auto date_time_fmt_expanded = "YYYY-MM-DD HH:MM:SS"sv;
  static constexpr std::uint32_t date_time_fmt_size =
    std::size(date_time_fmt_expanded);
  static constexpr auto delim = ' ';
  static constexpr auto date_time_fmt = "{:%F}{}{:%T}";
  static constexpr auto level_name = std::array{
    // clang-format off
    "DEBUG"sv,
    "INFO"sv,
    "WARNING"sv,
    "ERROR"sv,
    "CRITICAL"sv,
    // clang-format on
  };

public:
  static constexpr log_level_t default_level{log_level_t::info};

  static auto
  instance(const std::shared_ptr<std::ostream> &log_file_ptr = nullptr,
           const std::string &appname = "",
           log_level_t min_log_level = log_level_t::debug) -> logger & {
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

  [[nodiscard]] static auto
  set_level(const log_level_t lvl) noexcept {
    instance().min_log_level = lvl;
  }

  operator bool() const { return status ? false : true; }

  template <log_level_t the_level>
  auto
  log(const std::string_view msg) -> void {
    if (the_level >= min_log_level) {
      std::lock_guard lck{mtx};
      format_time();
      const auto end_pos = fill_buffer<the_level>(msg);
      log_file->write(buf.data(), std::distance(buf.data(), end_pos));
      log_file->flush();
    }
  }

  template <log_level_t the_level, typename... Args>
  auto
  log(const std::string_view fmt_str, Args &&...args) -> void {
    if (the_level >= min_log_level) {
      const auto msg = std::vformat(fmt_str, std::make_format_args(args...));
      std::lock_guard lck{mtx};
      format_time();
      const auto end_pos = fill_buffer<the_level>(msg);
      log_file->write(buf.data(), std::distance(buf.data(), end_pos));
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
  std::shared_ptr<std::ostream> log_file{nullptr};
  std::mutex mtx{};
  char *cursor{};
  log_level_t min_log_level{};
  std::error_code status{};

  logger(const logger &) = delete;
  auto
  operator=(const logger &) -> logger & = delete;
  logger() = default;
  ~logger() = default;

  template <log_level_t the_level>
  [[nodiscard]] auto
  fill_buffer(const std::string_view msg) -> char * {
    static constexpr auto lvl = level_name[std::to_underlying(the_level)];
    static constexpr auto sz = std::size(lvl);
    auto buf_data_itr = cursor;
    std::memcpy(buf_data_itr, lvl.data(), sz);
    buf_data_itr += sz;
    *buf_data_itr++ = delim;
    std::memcpy(buf_data_itr, msg.data(), std::size(msg));
    buf_data_itr += std::size(msg);
    *buf_data_itr++ = '\n';
    return buf_data_itr;
  }

  auto
  format_time() -> void {
    const auto now = std::chrono::system_clock::now();
    const auto now_time_t = std::chrono::system_clock::to_time_t(now);
    struct tm tm;
    localtime_r(&now_time_t, &tm);  // localtime_r is thread-safe
    std::strftime(buf.data(), std::size(buf), "%Y-%m-%d %H:%M:%S", &tm);
    buf[date_time_fmt_size] = delim;  // without this, we always get a
                                      // '\0' in the log message
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
  format(const transferase::log_level_t &lvl, std::format_context &ctx) const {
    const auto l = std::to_underlying(lvl);
    return std::format_to(ctx.out(), "{}", transferase::level_name[l]);
  }
};

#endif  // LIB_INCLUDE_LOGGER_HPP_
