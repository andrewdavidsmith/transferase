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
#include <format>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <variant>  // for std::tuple

using namespace std::chrono_literals;  // NOLINT

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
  std::chrono::microseconds timeout{10'000'000};
  bool show_progress{};

  download_request(const std::string &host, const std::string &port,
                   const std::string &target, const std::string &outdir,
                   const bool show_progress = false) :
    host{host}, port{port}, target{target}, outdir{outdir},
    show_progress{show_progress} {}

  download_request(const std::string &host, const std::string &port,
                   const std::string &target, const std::string &outdir,
                   const std::chrono::microseconds timeout,
                   const bool show_progress = false) :
    host{host}, port{port}, target{target}, outdir{outdir}, timeout{timeout},
    show_progress{show_progress} {}

  template <typename T>
  auto
  set_timeout(const T d) -> void
    requires chrono_duration<T>
  {
    // cppcheck-suppress missingReturn
    timeout = d;
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

template <>
struct std::formatter<transferase::download_request>
  : std::formatter<std::string> {
  auto
  format(const transferase::download_request &x, auto &ctx) const {
    return std::formatter<std::string>::format(
      std::format("{}:{}{} {} {}", x.host, x.port, x.target, x.outdir,
                  x.timeout),
      ctx);
  }
};

#endif  // LIB_DOWNLOAD_HPP_
