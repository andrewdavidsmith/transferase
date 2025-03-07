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

#ifndef LIB_DOWNLOAD_HPP_
#define LIB_DOWNLOAD_HPP_

#include <chrono>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <variant>  // for std::tuple

#include "httplib/httplib.h"

namespace transferase {

template <typename T>
concept chrono_duration = requires(T t) {
  typename T::rep;
  typename T::period;
};

struct download_request {
  std::string host;
  std::string port;
  std::string target;
  std::string outdir;
  std::chrono::milliseconds connect_timeout{10'000'000};    // milliseconds
  std::chrono::milliseconds download_timeout{240'000'000};  // milliseconds
  bool show_progress{};

  download_request(const std::string &host, const std::string &port,
                   const std::string &target, const std::string &outdir,
                   const bool show_progress = false) :
    host{host}, port{port}, target{target}, outdir{outdir},
    show_progress{show_progress} {}

  download_request(const std::string &host, const std::string &port,
                   const std::string &target, const std::string &outdir,
                   const std::chrono::milliseconds connect_timeout_in_millis,
                   const std::chrono::milliseconds download_timeout_in_millis,
                   const bool show_progress = false) :
    host{host}, port{port}, target{target}, outdir{outdir},
    connect_timeout{connect_timeout_in_millis},
    download_timeout{download_timeout_in_millis}, show_progress{show_progress} {
  }

  template <typename T>
  auto
  set_connect_timeout(T const d) -> void
    requires chrono_duration<T>
  {
    // cppcheck-suppress missingReturn
    connect_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(d);
  }

  template <typename T>
  auto
  set_download_timeout(T const d) -> void
    requires chrono_duration<T>
  {
    // cppcheck-suppress missingReturn
    download_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(d);
  }

  auto
  set_connect_timeout(auto const d) -> void {
    connect_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::seconds(d));
  }

  auto
  set_download_timeout(auto const d) -> void {
    download_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::seconds(d));
  }

  auto
  get_connect_timeout() const -> std::chrono::milliseconds {
    return connect_timeout;
  }

  auto
  get_download_timeout() const -> std::chrono::milliseconds {
    return download_timeout;
  }
};

[[nodiscard]]
auto
download(const download_request &dr)
  -> std::tuple<std::unordered_map<std::string, std::string>, std::error_code>;

[[nodiscard]]
auto
get_timestamp(const download_request &dr)
  -> std::chrono::time_point<std::chrono::file_clock>;

}  // namespace transferase

/// @brief Enum for error codes related to http
enum class http_error_code : std::uint8_t {
  Success = 0,
  Unknown,
  Connection,
  BindIPAddress,
  Read,
  Write,
  ExceedRedirectCount,
  Canceled,
  SSLConnection,
  SSLLoadingCerts,
  SSLServerVerification,
  SSLServerHostnameVerification,
  UnsupportedMultipartBoundaryChars,
  Compression,
  ConnectionTimeout,
  ProxyConnection,
};

template <>
struct std::is_error_code_enum<http_error_code> : public std::true_type {};

struct http_error_category : std::error_category {
  auto
  name() const noexcept -> const char * override {
    return "http";
  }
  auto
  message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case std::to_underlying(http_error_code::Success):
      return "Success"s;
    case std::to_underlying(http_error_code::Unknown):
      return "Unknown"s;
    case std::to_underlying(http_error_code::Connection):
      return "Connection"s;
    case std::to_underlying(http_error_code::BindIPAddress):
      return "BindIPAddress"s;
    case std::to_underlying(http_error_code::Read):
      return "Read"s;
    case std::to_underlying(http_error_code::Write):
      return "Write"s;
    case std::to_underlying(http_error_code::ExceedRedirectCount):
      return "ExceedRedirectCount"s;
    case std::to_underlying(http_error_code::Canceled):
      return "Canceled"s;
    case std::to_underlying(http_error_code::SSLConnection):
      return "SSLConnection"s;
    case std::to_underlying(http_error_code::SSLLoadingCerts):
      return "SSLLoadingCerts"s;
    case std::to_underlying(http_error_code::SSLServerVerification):
      return "SSLServerVerification"s;
    case std::to_underlying(http_error_code::SSLServerHostnameVerification):
      return "SSLServerHostnameVerification"s;
    case std::to_underlying(http_error_code::UnsupportedMultipartBoundaryChars):
      return "UnsupportedMultipartBoundaryChars"s;
    case std::to_underlying(http_error_code::Compression):
      return "Compression"s;
    case std::to_underlying(http_error_code::ConnectionTimeout):
      return "ConnectionTimeout"s;
    case std::to_underlying(http_error_code::ProxyConnection):
      return "ProxyConnection"s;
    }
    std::unreachable();
  }
};

inline auto
make_error_code(http_error_code e) -> std::error_code {
  static auto category = http_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // LIB_DOWNLOAD_HPP_
