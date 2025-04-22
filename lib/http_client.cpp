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
#include <asio/ssl.hpp>  // IWYU pragma: keep

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>

namespace transferase {

using namespace std::chrono_literals;

template <typename derived> class http_client_base {
public:
  http_client_base(const std::string &server, const std::string &port,
                   const std::string &target, const std::string &progress_label,
                   const bool header_only = false) :
    resolver{ioc}, server{server}, port{port}, target{target},
    header_only{header_only}, watchdog_timer{ioc},
    progress_label{progress_label}, progress{progress_label} {}

  auto
  download() -> void {
    resolve();
    ioc.run();
  }

  [[nodiscard]] auto
  get_status() const -> std::error_code {
    return status;
  }

  [[nodiscard]] auto
  take_data() const -> std::vector<char> {
    return std::move(buf);
  }

  [[nodiscard]] auto
  get_header() const -> http_header {
    return header;
  }

  auto
  set_timeout(const auto t) {
    duration = t;
  }

  // private:
  auto
  reset_deadline() -> void {
    deadline = std::chrono::steady_clock::now() + duration;
  }

  auto
  watchdog() -> void {
    watchdog_timer.expires_at(deadline);
    watchdog_timer.async_wait([this](auto) {
      if (!is_stopped()) {
        if (deadline < std::chrono::steady_clock::now())
          stop(http_error_code::inactive_timeout);
        else
          watchdog();
      }
    });
  }

  [[nodiscard]] auto
  is_stopped() const -> bool {
    return !self().get_sock().is_open();
  }

  auto
  stop(std::error_code ec) -> void {
    status = ec;
    (void)self().get_sock().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    (void)self().get_sock().lowest_layer().close(ec);
    watchdog_timer.cancel();
  }

  auto
  resolve() -> void {
    resolver.async_resolve(server, port,
                           [this](const auto &ec, const auto &res) {
                             if (ec)
                               stop(http_error_code::connect_failed);
                             else
                               connect(res);
                           });
  }

  auto
  connect(const auto &resolved) -> void {
    asio::async_connect(self().get_sock().lowest_layer(), resolved,
                        [this](const auto ec, auto) {
                          if (ec)
                            stop(http_error_code::connect_failed);
                          else
                            self().connect_handler();
                        });
    reset_deadline();
    watchdog();
  }

  auto
  process_header(const std::error_code ec, const std::size_t n_bytes) {
    if (ec) {
      stop(http_error_code::receive_header_failed);
      return;
    }

    header = http_header(std::string(buf.data(), n_bytes));
    if (header_only) {
      stop(std::error_code{});
      return;
    }
    if (header.content_length == 0) {
      stop(http_error_code::unknown_content_length);
      return;
    }
    buf_pos = std::size(buf) - n_bytes;
    std::memcpy(buf.data(), buf.data() + n_bytes, buf_pos);
    buf.resize(header.content_length);
    content_remaining = header.content_length - buf_pos;

    if (!progress_label.empty())
      progress.set_total_size(content_remaining);

    self().read_content();
  }

  [[nodiscard]] auto
  self() -> derived & {
    return static_cast<derived &>(*this);
  }

  [[nodiscard]] auto
  self() const -> const derived & {
    return static_cast<const derived &>(*this);
  }

  asio::io_context ioc;
  asio::ip::tcp::resolver resolver;

  const std::string server;
  const std::string port;
  const std::string target;
  const bool header_only{false};

  std::chrono::steady_clock::time_point deadline{};
  std::chrono::microseconds duration{1s};
  asio::steady_timer watchdog_timer;

  http_header header;
  std::vector<char> buf;
  std::size_t buf_pos{};
  std::size_t content_remaining{};

  std::string progress_label;
  download_progress progress;

  std::error_code status{};
};

class http_client : public http_client_base<http_client> {
public:
  http_client(const std::string &server, const std::string &port,
              const std::string &target, const std::string &progress_label,
              const bool header_only = false) :
    http_client_base(server, port, target, progress_label, header_only),
    sock{http_client_base::ioc} {}

  auto
  connect_handler() -> void {
    send_request();
  }

  [[nodiscard]] auto
  get_sock() -> asio::ip::tcp::socket & {
    return sock;
  }

  [[nodiscard]] auto
  get_sock() const -> const asio::ip::tcp::socket & {
    return sock;
  }

  auto
  read_header() -> void {
    reset_deadline();
    asio::async_read_until(sock, asio::dynamic_buffer(buf), "\r\n\r\n",
                           std::bind(&http_client_base::process_header, this,
                                     std::placeholders::_1,
                                     std::placeholders::_2));
  }

  auto
  read_content() -> void {
    reset_deadline();
    asio::async_read(
      sock, asio::buffer(buf.data() + buf_pos, content_remaining),
      [this](const auto ec, const auto n_bytes) -> std::size_t {
        // custom completion condition updates deadline
        reset_deadline();
        if (!progress_label.empty())
          progress.update(n_bytes);
        return is_stopped() ? 0 : asio::transfer_all()(ec, n_bytes);
      },
      [this](const auto ec, const auto n_bytes) {
        if (!progress_label.empty())
          progress.update(n_bytes);
        if (ec && (ec != asio::error::eof || n_bytes != content_remaining))
          stop(http_error_code::reading_content_failed);
        else
          stop(std::error_code{});
      });
  }

  auto
  send_request() -> void {
    static constexpr auto req_fmt = "GET {} HTTP/1.1\r\nHost: {}\r\n\r\n";
    const auto req = std::format(req_fmt, target, server);
    reset_deadline();
    asio::async_write(sock, asio::buffer(req), [this](const auto ec, auto) {
      if (ec)
        stop(http_error_code::send_request_failed);
      else
        read_header();
    });
  }

  asio::ip::tcp::socket sock;
};

class https_client : public http_client_base<https_client> {
public:
  https_client(const std::string &server, const std::string &port,
               const std::string &target, const std::string &progress_label,
               asio::ssl::context &ssl_context,
               const bool header_only = false) :
    http_client_base(server, port, target, progress_label, header_only),
    sock{http_client_base::ioc, ssl_context} {
    sock.set_verify_mode(asio::ssl::verify_none);
    sock.set_verify_callback(
      [](const auto preverified, asio::ssl::verify_context &) {
        return preverified;
      });
  }

  auto
  connect_handler() -> void {
    sock.async_handshake(asio::ssl::stream_base::client, [this](const auto ec) {
      if (ec)
        stop(http_error_code::handshake_failed);
      else
        send_request();
    });
  }

  [[nodiscard]] auto
  get_sock() -> asio::ip::tcp::socket & {
    return sock.next_layer();
  }

  [[nodiscard]] auto
  get_sock() const -> const asio::ip::tcp::socket & {
    return sock.next_layer();
  }

  auto
  read_header() -> void {
    reset_deadline();
    asio::async_read_until(sock, asio::dynamic_buffer(buf), "\r\n\r\n",
                           std::bind(&http_client_base::process_header, this,
                                     std::placeholders::_1,
                                     std::placeholders::_2));
  }

  auto
  read_content() -> void {
    reset_deadline();
    asio::async_read(
      sock, asio::buffer(buf.data() + buf_pos, content_remaining),
      [this](const auto ec, const auto n_bytes) -> std::size_t {
        // custom completion condition is to update deadline
        reset_deadline();
        progress.update(n_bytes);
        return is_stopped() ? 0 : asio::transfer_all()(ec, n_bytes);
      },
      [this](const auto ec, const auto n_bytes) {
        progress.update(n_bytes);
        if (ec && (ec != asio::error::eof || n_bytes != content_remaining))
          stop(http_error_code::reading_content_failed);
        else
          stop(std::error_code{});
      });
  }

  auto
  send_request() -> void {
    static constexpr auto req_fmt = "GET {} HTTP/1.1\r\nHost: {}\r\n\r\n";
    const auto req = std::format(req_fmt, target, server);
    reset_deadline();
    asio::async_write(sock, asio::buffer(req), [this](const auto ec, auto) {
      if (ec)
        stop(http_error_code::send_request_failed);
      else
        read_header();
    });
  }

  asio::ssl::stream<asio::ip::tcp::socket> sock;
};

[[nodiscard]] auto
download_http(const std::string &host, const std::string &port,
              const std::string &target, const std::string &outfile,
              const std::chrono::microseconds timeout, const bool show_progress)
  -> std::tuple<http_header, std::error_code> {

  std::string progress_label;
  if (show_progress)
    progress_label = std::filesystem::path(target).filename().string();

  const auto [data, header, status] = [&] {
    if (port == "443") {
      // ADS: verification currently disabled
      asio::ssl::context ctx(asio::ssl::context::sslv23);
      // ctx.load_verify_file("ca.pem");
      https_client c(host, port, target, progress_label, ctx);
      c.set_timeout(timeout);
      c.download();
      return std::tuple{c.take_data(), c.get_header(), c.get_status()};
    }
    else {
      http_client c(host, port, target, progress_label);
      c.set_timeout(timeout);
      c.download();
      return std::tuple{c.take_data(), c.get_header(), c.get_status()};
    }
  }();

  if (status)
    return std::tuple{http_header{}, status};

  std::ofstream out(outfile);
  if (!out)
    return {http_header{}, std::make_error_code(std::errc(errno))};

  out.write(data.data(), std::ssize(data));

  return {header, status};
}

[[nodiscard]] auto
download_header_http(const std::string &host, const std::string &port,
                     const std::string &target,
                     const std::chrono::microseconds timeout) -> http_header {
  if (port == "443") {
    // ADS: verification currently disabled
    asio::ssl::context ctx(asio::ssl::context::sslv23);
    // ctx.load_verify_file("ca.pem");
    https_client c(host, port, target, "", ctx, true);
    c.set_timeout(timeout);
    c.download();
    return c.get_header();
  }
  else {
    http_client c(host, port, target, "", true);
    c.set_timeout(timeout);
    c.download();
    return c.get_header();
  }
}

};  // namespace transferase
