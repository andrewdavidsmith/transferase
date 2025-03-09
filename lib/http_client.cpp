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

#include "http_client.hpp"

#include "download_progress.hpp"
#include "http_error_code.hpp"
#include "http_header.hpp"

#include <asio.hpp>

#include <algorithm>
#include <cerrno>
#include <compare>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>

namespace transferase {

[[nodiscard]] auto
download_http(
  const std::string &host, const std::string &port, const std::string &target,
  const std::string &outfile, const std::chrono::microseconds connect_timeout,
  const std::chrono::microseconds download_timeout,
  const bool show_progress) -> std::tuple<http_header, std::error_code> {
  asio::io_context io_context;

  asio::ip::tcp::resolver resolver(io_context);
  auto endpoints = resolver.resolve(host, port);

  std::string progress_label;
  if (show_progress)
    progress_label = std::filesystem::path(target).filename().string();

  http_client c(io_context, endpoints, host, port, target, connect_timeout,
                download_timeout, progress_label);

  io_context.run();

  const std::error_code error = c.get_status();
  if (error)
    return {{}, error};

  const auto &data = c.get_data();
  std::ofstream out(outfile);
  if (!out)
    return {{}, std::make_error_code(std::errc(errno))};

  out.write(data.data(), std::ssize(data));

  return {c.get_header(), std::error_code{}};
}

[[nodiscard]] auto
download_header_http(
  const std::string &host, const std::string &port, const std::string &target,
  const std::chrono::microseconds connect_timeout,
  const std::chrono::microseconds download_timeout) -> http_header {
  asio::io_context io_context;
  asio::ip::tcp::resolver resolver(io_context);
  auto endpoints = resolver.resolve(host, port);
  http_client c(io_context, endpoints, host, port, target, connect_timeout,
                download_timeout, true);
  io_context.run();
  return c.get_header();
}

http_client::http_client(asio::io_context &io_context,
                         const asio::ip::tcp::resolver::results_type &endpoints,
                         const std::string &host, const std::string &port,
                         const std::string &target,
                         const std::chrono::microseconds connect_timeout,
                         const std::chrono::microseconds read_timeout) :
  sock(io_context), host{host}, port{port}, target{target},
  connect_timeout{connect_timeout}, read_timeout{read_timeout},
  deadline{sock.get_executor()} {
  connect(endpoints);
  deadline.expires_after(connect_timeout);
  deadline.async_wait([this](auto) { check_deadline(); });
}

http_client::http_client(asio::io_context &io_context,
                         const asio::ip::tcp::resolver::results_type &endpoints,
                         const std::string &host, const std::string &port,
                         const std::string &target,
                         const std::chrono::microseconds connect_timeout,
                         const std::chrono::microseconds read_timeout,
                         const std::string &progress_label) :
  sock(io_context), host{host}, port{port}, target{target},
  connect_timeout{connect_timeout}, read_timeout{read_timeout},
  progress_label{progress_label}, progress(progress_label),
  deadline{sock.get_executor()} {
  connect(endpoints);
  deadline.expires_after(connect_timeout);
  deadline.async_wait([this](auto) { check_deadline(); });
}

http_client::http_client(asio::io_context &io_context,
                         const asio::ip::tcp::resolver::results_type &endpoints,
                         const std::string &host, const std::string &port,
                         const std::string &target,
                         const std::chrono::microseconds connect_timeout,
                         const std::chrono::microseconds read_timeout,
                         const bool header_only) :
  sock(io_context), host{host}, port{port}, target{target},
  header_only{header_only}, connect_timeout{connect_timeout},
  read_timeout{read_timeout}, deadline{sock.get_executor()} {
  connect(endpoints);
  deadline.expires_after(connect_timeout);
  deadline.async_wait([this](auto) { check_deadline(); });
}

auto
http_client::allocate_buffer(const std::size_t file_size) -> void {
  bytes_remaining = file_size;
  bytes_received = 0;
  buf.resize(bytes_remaining);
  std::copy_n(asio::buffers_begin(response.data()), std::size(response),
              std::begin(buf));
  bytes_remaining -= std::size(response);
  bytes_received += std::size(response);
}

auto
http_client::connect(const asio::ip::tcp::resolver::results_type &endpoints)
  -> void {
  deadline.expires_after(connect_timeout);
  asio::async_connect(
    sock.lowest_layer(), endpoints,
    [this](const std::error_code &error,
           const asio::ip::tcp::endpoint & /*endpoint*/) {
      deadline.expires_at(asio::steady_timer::time_point::max());
      if (!error) {
        const auto endpoint_str =
          (std::ostringstream() << sock.lowest_layer().remote_endpoint()).str();
#ifdef DEBUG
        std::println("Connected to {}", endpoint_str);
#endif
        send_request();
      }
      else {
#ifdef DEBUG
        std::println("Connect failed: {}", error.message());
#endif
        finish(http_error_code::connect_failed);
      }
    });
}

auto
http_client::send_request() -> void {
  constexpr auto get_fmt = "GET {} HTTP/1.1\r\nHost: {}\r\n\r\n";
  request = std::format(get_fmt, target, host);
  deadline.expires_after(connect_timeout);
  asio::async_write(
    sock, asio::buffer(request),
    [this](const std::error_code &error, const std::size_t /*size*/) {
      deadline.expires_at(asio::steady_timer::time_point::max());
      if (error) {
#ifdef DEBUG
        std::println("Error sending GET {}", error.message());
#endif
        finish(http_error_code::send_request_failed);
      }
      else {
        receive_header();
      }
    });
}

auto
http_client::receive_header() -> void {
  deadline.expires_after(read_timeout);
  asio::async_read_until(
    sock, response, http_end,
    [this](const std::error_code &error, const std::size_t size) {
      deadline.expires_at(asio::steady_timer::time_point::max());
      if (error) {
#ifdef DEBUG
        std::println("Error receiving GET header {}", ec.message());
#endif
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
#ifdef DEBUG
          std::println("Unknown body length");
#endif
          finish(http_error_code::unknown_body_length);
        }
      }
    });
}

auto
http_client::receive_body() -> void {
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
#ifdef DEBUG
        std::println("Error reading: {} ({})", error.message(),
                     bytes_transferred);
#endif
        finish(http_error_code::reading_body_failed);
      }
    });
}

auto
http_client::finish(const std::error_code error) -> void {
  // same consequence as canceling
  deadline.expires_at(asio::steady_timer::time_point::max());
  if (!status)  // could have been a timeout
    status = error;
  std::error_code unused_error;  // for non-throwing
  // nothing actually returned below
  (void)sock.shutdown(asio::ip::tcp::socket::shutdown_both, unused_error);
  // nothing actually returned below
  (void)sock.close(unused_error);
}

auto
http_client::check_deadline() -> void {
  if (!sock.is_open())  // ADS: when can this happen?
    return;

  if (const auto right_now = asio::steady_timer::clock_type::now();
      deadline.expiry() <= right_now) {
    status = http_error_code::inactive_timeout;
    std::error_code unused_error;  // for non-throwing
    // nothing actually returned below
    (void)sock.shutdown(asio::ip::tcp::socket::shutdown_both, unused_error);
    deadline.expires_at(asio::steady_timer::time_point::max());

    /* ADS: closing here if needed?? */
    (void)sock.close(unused_error);
  }

  // wait again
  deadline.async_wait([this](auto) { check_deadline(); });
}

};  // namespace transferase
