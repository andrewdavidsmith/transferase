/* MIT License
 *
 * Copyright (c) 2024-2025 Andrew D Smith
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

#ifndef LIB_CLIENT_CONNECTION_HPP_
#define LIB_CLIENT_CONNECTION_HPP_

#include "customize_asio.hpp"
#include "level_container.hpp"
#include "libdeflate_adapter.hpp"
#include "logger.hpp"
#include "query_container.hpp"
#include "request.hpp"
#include "response.hpp"
#include "server_error_code.hpp"
#include "transfer_stats.hpp"

#include <asio.hpp>

#include <config.h>

#include <chrono>
#include <cstddef>  // std::size_t
#include <cstdint>  // std::uint32_t
#include <string>
#include <system_error>
#include <utility>  // std::move
#include <vector>

namespace transferase {

template <typename derived, typename level_t>
class client_connection
  : std::enable_shared_from_this<client_connection<derived, level_t>> {
  using tcp = asio::ip::tcp;

public:
  client_connection(const std::string &hostname, const std::string &port,
                    const request &req) :
    resolver(ioc), socket(ioc), watchdog_timer{socket.get_executor()},
    hostname{hostname}, port{port}, req{req}, lgr{logger::instance()} {}

  // clang-format off
  client_connection(const client_connection &) = delete;
  auto operator=(const client_connection &) -> client_connection & = delete;
  client_connection(client_connection &&) = delete;
  auto operator=(client_connection &&) -> client_connection & = delete;
  ~client_connection() = default;
  // clang-format on

  [[nodiscard]] auto
  run() -> std::error_code {
    compose_request();
    ioc.run();
    return status;
  }

  [[nodiscard]] auto
  take_levels() -> level_container<level_t> {
    if (need_decompression() && !status) {
      // NOLINTNEXTLINE (*-reinterpret-cast)
      const auto buf = reinterpret_cast<std::uint8_t *>(levels_buffer());
      std::vector<std::uint8_t> tmp(buf, buf + resp_hdr.n_bytes);
      status = libdeflate_decompress(tmp, resp_container);
    }
    return std::move(resp_container);
  }

  [[nodiscard]] auto
  is_stopped() const -> bool {
    return !socket.is_open();
  }

  auto
  reset_deadline() -> void {
    static constexpr std::chrono::seconds comm_timeout_sec{10};  // NOLINT
    deadline = std::chrono::steady_clock::now() + comm_timeout_sec;
  }

  auto
  wait_while_server_works() -> void {
    static constexpr std::chrono::seconds work_timeout_sec{300};  // NOLINT
    deadline = std::chrono::steady_clock::now() + work_timeout_sec;
  }

  auto
  handle_write_failure(const std::error_code ec) -> void {
    lgr.debug("Error writing request: {}", ec.message());
    reset_deadline();
    asio::async_read(
      socket, asio::buffer(resp_hdr_buf), asio::transfer_exactly(resp_hdr_sz),
      [this](const auto ec, auto) {
        if (ec) {
          lgr.debug("Error reading response header: {}", ec.message());
          stop(ec);
          return;
        }
        if (const auto parse_err = parse(resp_hdr_buf, resp_hdr); parse_err) {
          lgr.debug("Error parsing response header: {}", parse_err.message());
          stop(parse_err);
          return;
        }
        lgr.debug("Failure explanation: {}", resp_hdr.summary());
        stop(resp_hdr.status);
      });
  }

  auto
  read_response_header() -> void {
    wait_while_server_works();
    asio::async_read(
      socket, asio::buffer(resp_hdr_buf), asio::transfer_exactly(resp_hdr_sz),
      [this](const auto ec, auto) {
        if (ec) {
          lgr.debug("Error reading response header: {}", ec.message());
          stop(ec);
          return;
        }
        parse_response_header();
      });
  }

  auto
  read_response_payload() -> void {
    resp_container.resize(resp_hdr.rows, resp_hdr.cols);
    reset_deadline();
    asio::async_read(
      socket, asio::buffer(levels_buffer(), resp_hdr.n_bytes),
      [this](const auto ec, const auto n_bytes) -> std::size_t {
        reply_stats.update(n_bytes);
        reset_deadline();
        auto completion_condition = transfer_all_higher_max();
        return is_stopped() ? 0 : completion_condition(ec, n_bytes);
      },
      [this](const auto ec, const auto n_bytes) {
        reply_stats.update(n_bytes);
        reset_deadline();
        lgr.debug("Response transfer stats: {}", reply_stats.str());
        if (ec)
          lgr.error("Error reading levels: {}", ec.message());
        stop(ec);
      });
  }

private:
  auto
  compose_request() -> void {
    if (const auto comp_err = compose(req_buf, req); comp_err) {
      lgr.debug("Error forming request: {}", comp_err.message());
      status = comp_err;
      return;
    }
    resolve();
  }

  auto
  resolve() -> void {
    std::error_code ec;
    asio::ip::make_address(hostname, ec);
    const auto flags =
      ec ? tcp::resolver::flags() : tcp::resolver::numeric_host;
    resolver.async_resolve(
      hostname, port, flags, [this](const auto &ec, const auto &res) {
        if (ec) {
          lgr.debug("Error resolving server address: {}", ec.message());
          stop(ec);
          return;
        }
        connect(res);
      });
  }

  auto
  connect(const tcp::resolver::results_type &resolved) -> void {
    const auto &ip_addr = std::cbegin(resolved)->endpoint().address();
    lgr.debug("Resolved server address: {}", ip_addr.to_string());

    reset_deadline();
    asio::async_connect(socket, resolved, [this](const auto ec, auto) {
      if (ec) {
        lgr.debug("Error connecting: {}", ec.message());
        stop(ec);
        return;
      }
      lgr.debug("Established connection with server");
      static_cast<derived *>(this)->write_request_header();
    });
    watchdog();  // start now: first connect to transferase server
  }

  auto
  parse_response_header() -> void {
    if (const auto parse_err = parse(resp_hdr_buf, resp_hdr); parse_err) {
      lgr.debug("Error parsing response header: {}", parse_err.message());
      stop(parse_err);
      return;
    }
    lgr.debug("Response header: {}", resp_hdr.summary());
    if (resp_hdr.xfr_version != VERSION)
      lgr.warning("Version mismatch (server={}, client={})",
                  resp_hdr.xfr_version, VERSION);
    if (resp_hdr.status) {
      stop(resp_hdr.status);
      return;
    }
    read_response_payload();
  }

  auto
  watchdog() -> void {
    watchdog_timer.expires_at(deadline);
    watchdog_timer.async_wait([this](auto) {
      if (!is_stopped()) {
        if (deadline < std::chrono::steady_clock::now())
          stop(server_error_code::connection_timeout);
        else
          watchdog();
      }
    });
  }

  auto
  stop(const std::error_code ec) -> void {
    status = ec;
    std::error_code unchecked;
    (void)socket.shutdown(tcp::socket::shutdown_both, unchecked);
    (void)socket.close(unchecked);
    watchdog_timer.cancel();
  }

  [[nodiscard]] auto
  need_decompression() const {
    // resp_hdr.n_bytes: number of bytes transferred; levels_size(): size of
    // structure computed; levels_size() might be smaller than bytes for
    // std::size(resp_container)
    return resp_hdr.n_bytes != levels_size();
  }

  [[nodiscard]] auto
  levels_size() const -> std::uint32_t {
    return sizeof(level_t) * resp_hdr.rows * resp_hdr.cols;
  }

  [[nodiscard]] auto
  levels_buffer() -> char * {
    return resp_container.data();
  }

private:
  asio::io_context ioc;
  tcp::resolver resolver;

public:
  tcp::socket socket;
  request_buffer req_buf{};

private:
  asio::steady_timer watchdog_timer;
  std::chrono::steady_clock::time_point deadline{};
  std::string hostname;
  std::string port;
  request req{};
  response_header_buffer resp_hdr_buf{};
  response_header resp_hdr{};
  level_container<level_t> resp_container{};
  std::error_code status{};
  transfer_stats reply_stats;

public:
  logger &lgr;
};  // class client_connection

template <typename level_t>
class intervals_client
  : public client_connection<intervals_client<level_t>, level_t> {
public:
  intervals_client(const std::string &hostname, const std::string &port,
                   const request &req, const query_container &query) :
    base_t(hostname, port, req), query{query} {}

  auto
  write_request_header() -> void {
    base_t::reset_deadline();
    asio::async_write(base_t::socket, asio::buffer(base_t::req_buf),
                      [this](const auto ec, auto) {
                        if (ec)
                          base_t::handle_write_failure(ec);
                        else
                          write_query();
                      });
  }

private:
  auto
  write_query() -> void {
    base_t::reset_deadline();
    asio::async_write(
      base_t::socket, asio::buffer(query.data(), query.n_bytes()),
      [this](const auto ec, const auto n_bytes) -> std::size_t {
        query_stats.update(n_bytes);
        base_t::reset_deadline();
        auto completion_condition = transfer_all_higher_max();
        return base_t::is_stopped() ? 0 : completion_condition(ec, n_bytes);
      },
      [this](const auto ec, const auto n_bytes) {
        query_stats.update(n_bytes);
        base_t::reset_deadline();
        base_t::lgr.debug("Sent query: {}", query_stats.str());
        if (ec)
          base_t::handle_write_failure(ec);
        else
          base_t::read_response_header();
      });
  }
  using base_t = client_connection<intervals_client<level_t>, level_t>;
  const query_container &query;
  transfer_stats query_stats;
};  // class intervals_client

template <typename level_t>
class bins_client : public client_connection<bins_client<level_t>, level_t> {
public:
  bins_client(const std::string &hostname, const std::string &port,
              const request &req) : base_t(hostname, port, req) {}

  auto
  write_request_header() -> void {
    base_t::reset_deadline();
    asio::async_write(base_t::socket, asio::buffer(base_t::req_buf),
                      [this](const auto ec, auto) {
                        if (ec)
                          base_t::handle_write_failure(ec);
                        else
                          base_t::read_response_header();
                      });
  }

private:
  using base_t = client_connection<bins_client<level_t>, level_t>;
};  // class bins_client

}  // namespace transferase

#endif  // LIB_CLIENT_CONNECTION_HPP_
