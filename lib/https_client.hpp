/* MIT License
 *
 * Copyright (c) 2025 Andrew D Smith
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

#ifndef LIB_HTTPS_CLIENT_HPP_
#define LIB_HTTPS_CLIENT_HPP_

#include "download_progress.hpp"
#include "http_header.hpp"

#include <asio.hpp>
#include <asio/ssl.hpp>  // IWYU pragma: keep

#include <chrono>
#include <cstdlib>
#include <string>
#include <system_error>
#include <tuple>
#include <variant>
#include <vector>

using namespace std::chrono_literals;

namespace transferase {

[[nodiscard]] auto
download_https(
  const std::string &host, const std::string &port, const std::string &target,
  const std::string &outfile, const std::chrono::microseconds connect_timeout,
  const std::chrono::microseconds download_timeout,
  const bool show_progress = false) -> std::tuple<http_header, std::error_code>;

[[nodiscard]] auto
download_header_https(
  const std::string &host, const std::string &port, const std::string &target,
  const std::chrono::microseconds connect_timeout,
  const std::chrono::microseconds download_timeout) -> http_header;

class https_client {
public:
  https_client(asio::io_context &io_context, asio::ssl::context &context,
               const asio::ip::tcp::resolver::results_type &endpoints,
               const std::string &host, const std::string &port,
               const std::string &target,
               const std::chrono::microseconds connect_timeout = 10'000ms,
               const std::chrono::microseconds read_timeout = 30'000ms);

  https_client(asio::io_context &io_context, asio::ssl::context &context,
               const asio::ip::tcp::resolver::results_type &endpoints,
               const std::string &host, const std::string &port,
               const std::string &target,
               const std::chrono::microseconds connect_timeout = 10'000ms,
               const std::chrono::microseconds read_timeout = 30'000ms,
               const std::string &progress_label = "");

  https_client(asio::io_context &io_context, asio::ssl::context &context,
               const asio::ip::tcp::resolver::results_type &endpoints,
               const std::string &host, const std::string &port,
               const std::string &target,
               const std::chrono::microseconds connect_timeout = 10'000ms,
               const std::chrono::microseconds read_timeout = 30'000ms,
               const bool header_only = false);

  [[nodiscard]] auto
  get_data() const -> const std::vector<char> & {
    return buf;
  }

  [[nodiscard]] auto
  get_header() const -> const http_header & {
    return header;
  }

  [[nodiscard]] auto
  get_status() const -> std::error_code {
    return status;
  }

private:
  auto
  allocate_buffer(const std::size_t file_size) -> void;

  auto
  verify_certificate(const bool preverified,
                     asio::ssl::verify_context &ctx) -> bool;

  auto
  check_deadline() -> void;

  auto
  connect(const asio::ip::tcp::resolver::results_type &endpoints) -> void;

  auto
  handshake() -> void;

  auto
  send_request() -> void;

  auto
  receive_header() -> void;

  auto
  receive_body() -> void;

  auto
  finish(const std::error_code error) -> void;

  static constexpr auto http_end = "\r\n\r\n";

  asio::ssl::stream<asio::ip::tcp::socket> sock;

  const std::string host;
  const std::string port;
  const std::string target;
  const bool header_only{false};

  /// Timeout for everything before reading the payload
  std::chrono::microseconds connect_timeout{10'000'000};
  /// Inactive timeout while reading the payload
  std::chrono::microseconds read_timeout{30'000'000};

  std::string progress_label;
  download_progress progress;

  std::string request;
  asio::streambuf response;
  http_header header;
  asio::steady_timer deadline;

  std::vector<char> buf;

  std::size_t bytes_received{};
  std::size_t bytes_remaining{};
  std::error_code status;
};

}  // namespace transferase

#endif  // LIB_HTTPS_CLIENT_HPP_
