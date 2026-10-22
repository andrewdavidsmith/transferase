// SPDX-License-Identifier: MIT; Copyright 2026 Andrew D Smith

#include "logger.hpp"

#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

namespace transferase {

logger::logger(const std::shared_ptr<std::ostream> &log_file,
               const std::string &appname, log_level_t min_log_level) :
  log_file{log_file}, min_log_level{min_log_level} {
  if (log_file == nullptr || !log_file->good()) {
    status = std::make_error_code(std::errc::bad_file_descriptor);
    return;
  }
  buf.fill(' ');  // ADS: not sure this is needed
  if (const auto ec = set_attributes(appname))
    status = ec;
}

[[nodiscard]] auto
logger::set_attributes(const std::string_view appname) -> std::error_code {
  // get the hostname
  static constexpr std::uint32_t max_hostname_size{256};
  std::string hostname;
  hostname.resize(max_hostname_size);
  if (gethostname(std::data(hostname), max_hostname_size))  // unistd.h
    return std::make_error_code(std::errc(errno));
  hostname.resize(hostname.find('\0'));

  // ADS TODO: error conditions on buffer sizes

  // set the end of the buffer; minus 1 because we use a newline in the buffer
  buf_lim = std::next(std::begin(buf), buf_size - 1);

  // NOLINTBEGIN(*-pointer-arithmetic)

  // fill the buffer (after the time fields)
  cursor = std::data(buf) + date_time_fmt_size;
  *cursor++ = delim;

  // hostname in buffer
  std::memcpy(cursor, std::data(hostname), std::size(hostname));
  cursor += std::size(hostname);
  *cursor++ = delim;

  // appname in buffer
  std::memcpy(cursor, std::data(appname), std::size(appname));
  cursor += std::size(appname);
  *cursor++ = delim;

  // process id in buffer
  std::to_chars_result tcr{cursor, std::errc()};
  const auto process_id = getpid();  // syscall;unistd.h
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wstringop-overflow=0"
#endif
  tcr = std::to_chars(tcr.ptr, std::end(buf), process_id);
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic pop
#endif
  // ADS: no complaints from gcc because it can't find buff?
  tcr.ptr = std::next(tcr.ptr);
  *tcr.ptr = delim;

  // NOLINTEND(*-pointer-arithmetic)

  if (tcr.ec != std::errc{})
    return std::make_error_code(tcr.ec);

  cursor = tcr.ptr;

  return std::make_error_code(std::errc{});
};

}  // namespace transferase
