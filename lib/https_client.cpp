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

#include "https_client.hpp"

#include "download_progress.hpp"
#include "http_error_code.hpp"
#include "http_header.hpp"

#include <asio.hpp>
#include <asio/ssl.hpp>  // IWYU pragma: keep

#include <algorithm>
#include <cerrno>
#include <compare>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>

namespace transferase {

[[nodiscard]] auto
download_https(
  const std::string &host, const std::string &port, const std::string &target,
  const std::string &outfile, const std::chrono::microseconds connect_timeout,
  const std::chrono::microseconds download_timeout,
  const bool show_progress) -> std::tuple<http_header, std::error_code> {
  asio::io_context io_context;

  asio::ip::tcp::resolver resolver(io_context);
  auto endpoints = resolver.resolve(host, port);

  // ADS: verification currently disabled
  asio::ssl::context ctx(asio::ssl::context::sslv23);
  // ctx.load_verify_file("ca.pem");

  std::string progress_label;
  if (show_progress)
    progress_label = std::filesystem::path(target).filename().string();
  https_client c(io_context, ctx, endpoints, host, port, target,
                 connect_timeout, download_timeout, progress_label);

  io_context.run();

  const std::error_code error = c.get_status();
  if (error)
    return {http_header{}, error};

  const auto &data = c.get_data();
  std::ofstream out(outfile);
  if (!out)
    return {http_header{}, std::make_error_code(std::errc(errno))};

  out.write(data.data(), std::ssize(data));

  return {c.get_header(), std::error_code{}};
}

[[nodiscard]] auto
download_header_https(
  const std::string &host, const std::string &port, const std::string &target,
  const std::chrono::microseconds connect_timeout,
  const std::chrono::microseconds download_timeout) -> http_header {
  asio::io_context io_context;
  asio::ip::tcp::resolver resolver(io_context);
  auto endpoints = resolver.resolve(host, port);

  // ADS: verification currently disabled
  asio::ssl::context ctx(asio::ssl::context::sslv23);
  // ctx.load_verify_file("ca.pem");

  https_client c(io_context, ctx, endpoints, host, port, target,
                 connect_timeout, download_timeout, true);
  io_context.run();
  return c.get_header();
}

https_client::https_client(
  asio::io_context &io_context, asio::ssl::context &context,
  const asio::ip::tcp::resolver::results_type &endpoints,
  const std::string &host, const std::string &port, const std::string &target,
  const std::chrono::microseconds connect_timeout,
  const std::chrono::microseconds read_timeout) :
  sock(io_context, context), host{host}, port{port}, target{target},
  connect_timeout{connect_timeout}, read_timeout{read_timeout},
  deadline{sock.get_executor()} {
  // ADS: currently disabled
  sock.set_verify_mode(asio::ssl::verify_none);
  sock.set_verify_callback(std::bind(&https_client::verify_certificate, this,
                                     std::placeholders::_1,
                                     std::placeholders::_2));

  connect(endpoints);
  deadline.expires_after(connect_timeout);
  deadline.async_wait([this](auto) { check_deadline(); });
}

https_client::https_client(
  asio::io_context &io_context, asio::ssl::context &context,
  const asio::ip::tcp::resolver::results_type &endpoints,
  const std::string &host, const std::string &port, const std::string &target,
  const std::chrono::microseconds connect_timeout,
  const std::chrono::microseconds read_timeout,
  const std::string &progress_label) :
  sock(io_context, context), host{host}, port{port}, target{target},
  connect_timeout{connect_timeout}, read_timeout{read_timeout},
  progress_label{progress_label}, progress(progress_label),
  deadline{sock.get_executor()} {
  // ADS: currently disabled
  sock.set_verify_mode(asio::ssl::verify_none);
  sock.set_verify_callback(std::bind(&https_client::verify_certificate, this,
                                     std::placeholders::_1,
                                     std::placeholders::_2));

  connect(endpoints);
  deadline.expires_after(connect_timeout);
  deadline.async_wait([this](auto) { check_deadline(); });
}

https_client::https_client(
  asio::io_context &io_context, asio::ssl::context &context,
  const asio::ip::tcp::resolver::results_type &endpoints,
  const std::string &host, const std::string &port, const std::string &target,
  const std::chrono::microseconds connect_timeout,
  const std::chrono::microseconds read_timeout, const bool header_only) :
  sock(io_context, context), host{host}, port{port}, target{target},
  header_only{header_only}, connect_timeout{connect_timeout},
  read_timeout{read_timeout}, deadline{sock.get_executor()} {
  // ADS: currently disabled
  sock.set_verify_mode(asio::ssl::verify_none);
  sock.set_verify_callback(std::bind(&https_client::verify_certificate, this,
                                     std::placeholders::_1,
                                     std::placeholders::_2));

  connect(endpoints);
  deadline.expires_after(connect_timeout);
  deadline.async_wait([this](auto) { check_deadline(); });
}

auto
https_client::allocate_buffer(const std::size_t file_size) -> void {
  bytes_remaining = file_size;
  bytes_received = 0;
  buf.resize(bytes_remaining);
  std::copy_n(asio::buffers_begin(response.data()), std::size(response),
              std::begin(buf));
  bytes_remaining -= std::size(response);
  bytes_received += std::size(response);
}

auto
https_client::verify_certificate(
  const bool preverified,
  [[maybe_unused]] asio::ssl::verify_context &ctx) -> bool {
  return preverified;
}

auto
https_client::connect(const asio::ip::tcp::resolver::results_type &endpoints)
  -> void {
  deadline.expires_after(connect_timeout);
  // clang-format off
  asio::async_connect(sock.lowest_layer(), endpoints,
    [this](const std::error_code &error, const auto & /*endpoint*/) {
      deadline.expires_at(asio::steady_timer::time_point::max());
      if (!error) {
        handshake();
      }
      else {
        finish(http_error_code::connect_failed);
      }
    });
  // clang-format on
}

auto
https_client::handshake() -> void {
  deadline.expires_after(connect_timeout);
  sock.async_handshake(
    asio::ssl::stream_base::client, [this](const std::error_code &error) {
      deadline.expires_at(asio::steady_timer::time_point::max());
      if (!error) {
        send_request();
      }
      else {
        finish(http_error_code::handshake_failed);
      }
    });
}

auto
https_client::send_request() -> void {
  constexpr auto get_fmt = "GET {} HTTP/1.1\r\nHost: {}\r\n\r\n";
  request = std::format(get_fmt, target, host);
  deadline.expires_after(connect_timeout);
  asio::async_write(
    sock, asio::buffer(request),
    [this](const std::error_code &error, const std::size_t /*size*/) {
      deadline.expires_at(asio::steady_timer::time_point::max());
      if (error) {
        finish(http_error_code::send_request_failed);
      }
      else {
        receive_header();
      }
    });
}

auto
https_client::receive_header() -> void {
  deadline.expires_after(read_timeout);
  asio::async_read_until(
    sock, response, http_end,
    [this](const std::error_code &error, const std::size_t size) {
      deadline.expires_at(asio::steady_timer::time_point::max());
      if (error) {
        finish(http_error_code::receive_header_failed);
      }
      else {
        header = http_header(response, size);
        // ADS: This is where the choice is made to just get headers
        if (header_only)
          finish(error);
        response.consume(size);
        if (header.content_length != 0) {
          allocate_buffer(header.content_length);
          if (!progress_label.empty())
            progress.set_total_size(header.content_length);
          receive_body();
        }
        else {
          finish(http_error_code::unknown_body_length);
        }
      }
    });
}

auto
https_client::receive_body() -> void {
  deadline.expires_after(read_timeout);
  sock.async_read_some(
    // NOLINTNEXTLINE (*-pointer-arithmetic)
    asio::buffer(buf.data() + bytes_received, bytes_remaining),
    [this](const std::error_code error, const std::size_t bytes_transferred) {
      deadline.expires_at(asio::steady_timer::time_point::max());
      if (!error) {
        bytes_remaining -= bytes_transferred;
        bytes_received += bytes_transferred;

        progress.update(bytes_received);

        if (bytes_remaining == 0) {
          finish(error);
        }
        else {
          receive_body();
        }
      }
      else {
        finish(http_error_code::reading_body_failed);
      }
    });
}

auto
https_client::finish(const std::error_code error) -> void {
  // same consequence as canceling
  deadline.expires_at(asio::steady_timer::time_point::max());
  if (!status)  // could have been a timeout
    status = error;
  std::error_code unused_error;  // for non-throwing
  // nothing actually returned below
  (void)sock.shutdown(unused_error);
  // nothing actually returned below
  (void)sock.lowest_layer().close(unused_error);
}

auto
https_client::check_deadline() -> void {
  if (!sock.lowest_layer().is_open())  // ADS: when can this happen?
    return;

  if (const auto right_now = asio::steady_timer::clock_type::now();
      deadline.expiry() <= right_now) {
    status = http_error_code::inactive_timeout;
    std::error_code shutdown_ec;  // for non-throwing
    // nothing actually returned below
    (void)sock.shutdown(shutdown_ec);
    deadline.expires_at(asio::steady_timer::time_point::max());

    /* ADS: closing here if needed?? */
    std::error_code socket_close_ec;  // for non-throwing
    // nothing actually returned below
    (void)sock.lowest_layer().close(socket_close_ec);
  }

  // wait again
  deadline.async_wait([this](auto) { check_deadline(); });
}

};  // namespace transferase
