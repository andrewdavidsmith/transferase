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

#include <chrono>
#include <cstdint>  // std::uint32_t
#include <cstring>  // std::memcpy
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>  // std::to_underlying

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

enum class mc16_log_level : std::uint32_t {
  debug,
  info,
  warning,
  error,
  n_levels,
};
static constexpr std::uint32_t mc16_n_log_levels =
  std::to_underlying(mc16_log_level::n_levels);

static constexpr std::array<const char *, mc16_n_log_levels> level_name = {
  "debug",
  "info",
  "warning",
  "error",
};

static constexpr auto level_name_sz{[]() constexpr {
  std::array<std::uint32_t, mc16_n_log_levels> tmp{};
  for (std::uint32_t i = 0; i < mc16_n_log_levels; ++i)
    tmp[i] = std::size(std::string_view(level_name[i]));
  return tmp;
}()};

inline std::ostream &
operator<<(std::ostream &o, const mc16_log_level &l) {
  return o << level_name[std::to_underlying(l)];
}

inline std::istream &
operator>>(std::istream &in, mc16_log_level &l) {
  std::string tmp;
  if (!(in >> tmp))
    return in;
  for (std::uint32_t i = 0; i < mc16_n_log_levels; ++i)
    if (tmp == std::string_view(level_name[i])) {
      l = static_cast<mc16_log_level>(i);
      return in;
    }
  in.setstate(std::ios::failbit);
  return in;
}

class logger {
private:
  static constexpr std::string_view date_time_fmt_expanded =
    "YYYY-MM-DD HH:MM:SS";
  static constexpr std::uint32_t date_time_fmt_size =
    std::size(date_time_fmt_expanded);
  static constexpr auto delim = ' ';
  static constexpr auto date_time_fmt = "{:%F}{}{:%T}";
  static constexpr std::array<const char *, mc16_n_log_levels> level_name = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
  };
  static constexpr auto level_name_sz{[]() constexpr {
    std::array<std::uint32_t, mc16_n_log_levels> tmp{};
    for (std::uint32_t i = 0; i < mc16_n_log_levels; ++i)
      tmp[i] = std::size(std::string_view(level_name[i]));
    return tmp;
  }()};

public:
  static logger &
  instance(std::shared_ptr<std::ostream> log_file_ptr = nullptr,
           std::string appname = "",
           mc16_log_level min_log_level = mc16_log_level::debug) {
    static logger fl(log_file_ptr, appname, min_log_level);
    return fl;
  }

  logger(std::shared_ptr<std::ostream> log_file_ptr, std::string appname,
         mc16_log_level min_log_level = mc16_log_level::info) :
    log_file{log_file_ptr}, min_log_level{min_log_level} {
    // ADS: check for log file good here?
    // status = std::make_error_code(std::errc(errno));
    buf.fill(' ');  // ADS: not sure this is needed
    if (const auto ec = set_attributes(appname))
      status = ec;
  }

  [[nodiscard]] auto get_status() const -> std::error_code { return status; }

  operator bool() const { return status ? false : true; }

  template <mc16_log_level the_level>
  auto log(std::string_view message) -> void {
    static constexpr auto idx = std::to_underlying(the_level);
    static constexpr auto sz = level_name_sz[idx];
    static constexpr auto lvl = level_name[idx];
    if (the_level >= min_log_level) {
      const auto thread_id =
        std::hash<std::thread::id>{}(std::this_thread::get_id());
      std::lock_guard lck{mtx};
      format_time();
      const auto buf_data_end = fill_buffer(thread_id, lvl, sz, message);
      log_file->write(buf.data(), std::distance(buf.data(), buf_data_end));
      log_file->flush();
    }
  }

  template <mc16_log_level the_level, typename... Args>
  auto log(std::string_view fmt_str, Args &&...args) -> void {
    static constexpr auto idx = std::to_underlying(the_level);
    static constexpr auto sz = level_name_sz[idx];
    static constexpr auto lvl = level_name[idx];
    if (the_level >= min_log_level) {
      const auto thread_id =
        std::hash<std::thread::id>{}(std::this_thread::get_id());
      const auto msg = std::vformat(fmt_str, std::make_format_args(args...));
      std::lock_guard lck{mtx};
      format_time();
      const auto buf_data_end = fill_buffer(thread_id, lvl, sz, msg);
      log_file->write(buf.data(), std::distance(buf.data(), buf_data_end));
      log_file->flush();
    }
  }

  auto debug(std::string_view message) -> void {
    log<mc16_log_level::debug>(message);
  }
  auto info(std::string_view message) -> void {
    log<mc16_log_level::info>(message);
  }
  auto warning(std::string_view message) -> void {
    log<mc16_log_level::warning>(message);
  }
  auto error(std::string_view message) -> void {
    log<mc16_log_level::error>(message);
  }

  template <typename... Args>
  auto debug(std::string_view fmt_str, Args &&...args) -> void {
    log<mc16_log_level::debug>(fmt_str, args...);
  }
  template <typename... Args>
  auto info(std::string_view fmt_str, Args &&...args) -> void {
    log<mc16_log_level::info>(fmt_str, args...);
  }
  template <typename... Args>
  auto warning(std::string_view fmt_str, Args &&...args) -> void {
    log<mc16_log_level::warning>(fmt_str, args...);
  }
  template <typename... Args>
  auto error(std::string_view fmt_str, Args &&...args) -> void {
    log<mc16_log_level::error>(fmt_str, args...);
  }

private:
  auto set_attributes(std::string_view) -> std::error_code;

  static constexpr std::uint32_t buf_size{1024};  // max log line
  std::array<char, buf_size> buf{};
  char *buf_end{buf.data() + buf_size};
  std::shared_ptr<std::ostream> log_file;
  std::mutex mtx{};
  char *cursor{};
  const mc16_log_level min_log_level{};
  std::error_code status{};

  logger(const logger &) = delete;
  logger &operator=(const logger &) = delete;

  logger() {}
  ~logger() {}

  [[nodiscard]]
  auto fill_buffer(std::uint32_t thread_id, const char *const lvl,
                   const std::uint32_t sz, std::string_view message) -> char * {
    auto [buf_data_end, ec] = std::to_chars(cursor, buf_end, thread_id);
    *buf_data_end++ = delim;
    std::memcpy(buf_data_end, lvl, sz);
    buf_data_end += sz;
    *buf_data_end++ = delim;
    return std::format_to(buf_data_end, "{}\n", message);
  }

  auto format_time() -> void {
    const auto now{std::chrono::system_clock::now()};
    const std::chrono::year_month_day ymd{
      std::chrono::floor<std::chrono::days>(now)};
    const std::chrono::hh_mm_ss hms{
      std::chrono::floor<std::chrono::seconds>(now) -
      std::chrono::floor<std::chrono::days>(now)};
    std::format_to(buf.data(), "{:%F}{}{:%T}", ymd, delim, hms);
  }
};  // struct logger

#endif  // SRC_LOGGER_HPP_