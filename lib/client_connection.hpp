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

#include "level_container_md.hpp"
#include "logger.hpp"
#include "query_container.hpp"
#include "request.hpp"
#include "response.hpp"
#include "server_error_code.hpp"
#include "transfer_stats.hpp"

#include <asio.hpp>

#include <algorithm>  // for std::min, std::max
#include <chrono>
#include <compare>  // for std::strong_ordering
#include <cstddef>  // for std::size_t
#include <cstdint>  // std::uint32_t
#include <limits>   // for std::numeric_limits
#include <string>
#include <system_error>
#include <utility>  // std::swap std::move
#include <vector>

namespace transferase {

template <typename derived, typename level_element>
class client_connection
  : std::enable_shared_from_this<client_connection<derived, level_element>> {
public:
  client_connection(const std::string &hostname, const std::string &port,
                    const request &req);

  // clang-format off
  client_connection(const client_connection &) = delete;
  auto operator=(const client_connection &) -> client_connection & = delete;
  client_connection(client_connection &&) = delete;
  auto operator=(client_connection &&) -> client_connection & = delete;
  ~client_connection() = default;
  // clang-format on

  auto
  run() -> std::error_code {
    io_context.run();
    return status;
  }

  [[nodiscard]] auto
  take_levels() -> level_container_md<level_element> {
    return std::move(resp_container);
  }

  [[nodiscard]] auto
  is_stopped() const -> bool {
    return !socket.is_open();
  }

  // clang-format off
  auto handle_write_failure(const std::error_code ec) -> void;
  auto read_response_header() -> void;
  auto handle_write_request(const std::error_code ec) -> void;
  // clang-format on

private:
  auto
  handle_resolve(const std::error_code ec,
                 const asio::ip::tcp::resolver::results_type &endpoints)
    -> void;

  // clang-format off
  auto handle_connect(const std::error_code ec) -> void;
  auto parse_response_header() -> void;
  auto read_response_payload() -> void;
  auto stop(std::error_code ec) -> void;
  auto watchdog() -> void;
  // clang-format on

  auto
  levels_size() const -> std::uint32_t {
    return sizeof(level_element) * resp_hdr.rows * resp_hdr.cols;
  }

  [[nodiscard]] auto
  levels_buffer() -> char * {
    return resp_container.data();
  }

public:
  auto
  set_deadline(const std::chrono::seconds delta) -> void {
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(delta);
  }

private:
  asio::io_context io_context;
  asio::ip::tcp::resolver resolver;

public:
  asio::ip::tcp::socket socket;
  asio::steady_timer watchdog_timer;
  std::chrono::steady_clock::time_point deadline{};
  request_buffer req_buf{};

private:
  request req{};
  response_header_buffer resp_hdr_buf{};
  response_header resp_hdr{};
  level_container_md<level_element> resp_container{};
  std::error_code status{};

public:
  logger &lgr;
  transfer_stats reply_stats;
  /// Timeout for individual read/write async ops
  std::chrono::seconds comm_timeout_sec{3};  // NOLINT
  /// Timeout when waiting for server to do work
  std::chrono::seconds work_timeout_sec{120};  // NOLINT
};  // class client_connection

template <typename D, typename L>
client_connection<D, L>::client_connection(const std::string &hostname,
                                           const std::string &port,
                                           const request &req) :
  resolver(io_context), socket(io_context),
  watchdog_timer{socket.get_executor()}, req{req}, lgr{logger::instance()} {
  const auto token = [this](const auto &error, const auto &results) {
    handle_resolve(error, results);
  };
  std::error_code ec;
  asio::ip::make_address(hostname, ec);
  if (ec) {  // hostname not an IP address
    lgr.debug("Resolving address for hostname: {}", hostname);
    resolver.async_resolve(hostname, port, token);
  }
  else {  // hostname given as IP address
    lgr.debug("Avoiding address resolution (ip: {})", hostname);
    const auto ns = asio::ip::resolver_query_base::numeric_service;
    resolver.async_resolve(hostname, port, ns, token);
  }
}

template <typename D, typename L>
auto
client_connection<D, L>::handle_resolve(
  const std::error_code ec,
  const asio::ip::tcp::resolver::results_type &endpoints) -> void {
  if (ec) {
    lgr.debug("Error resolving server: {}", ec.message());
    stop(ec);
  }
  else {
    set_deadline(comm_timeout_sec);
    asio::async_connect(socket, endpoints, [this](const auto &error, auto) {
      handle_connect(error);
    });
    watchdog();
  }
}

template <typename D, typename L>
auto
client_connection<D, L>::handle_connect(std::error_code ec) -> void {
  set_deadline(work_timeout_sec);
  if (ec) {
    lgr.debug("Error connecting: {}", ec.message());
    stop(ec);
    return;
  }

  const auto name = (std::ostringstream() << socket.remote_endpoint()).str();
  lgr.debug("Connected to server: {}", name);

  if (const auto comp_err = compose(req_buf, req); comp_err) {
    lgr.debug("Error forming request: {}", ec.message());
    stop(ec);
    return;
  }

  static_cast<D *>(this)->write_request_header();
}

template <typename D, typename L>
auto
client_connection<D, L>::handle_write_failure(const std::error_code ec)
  -> void {
  lgr.debug("Error writing request: {}", ec.message());
  set_deadline(comm_timeout_sec);
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

template <typename D, typename L>
auto
client_connection<D, L>::read_response_header() -> void {
  set_deadline(work_timeout_sec);
  // clang-format off
  asio::async_read(socket, asio::buffer(resp_hdr_buf),
                   asio::transfer_exactly(resp_hdr_sz),
    [this](const auto ec, auto) {
      if (ec) {
        lgr.debug("Error reading response header: {}", ec.message());
        stop(ec);
        return;
      }
      parse_response_header();
    });
  // clang-format on
}

template <typename D, typename L>
auto
client_connection<D, L>::parse_response_header() -> void {
  if (const auto parse_err = parse(resp_hdr_buf, resp_hdr); parse_err) {
    lgr.debug("Error parsing response header: {}", parse_err.message());
    stop(parse_err);
    return;
  }
  lgr.debug("Response header: {}", resp_hdr.summary());
  if (resp_hdr.status) {
    stop(resp_hdr.status);
    return;
  }
  read_response_payload();
}

template <typename D, typename L>
auto
client_connection<D, L>::read_response_payload() -> void {
  set_deadline(work_timeout_sec);
  resp_container.resize(resp_hdr.rows, resp_hdr.cols);
  asio::async_read(
    socket, asio::buffer(levels_buffer(), levels_size()),
    // completion condition
    [this](const auto ec, const auto n_bytes) -> std::size_t {
      auto completion_condition = asio::transfer_all();
      set_deadline(comm_timeout_sec);
      if (n_bytes > 0)
        reply_stats.update(n_bytes);
      return is_stopped() ? 0 : completion_condition(ec, n_bytes);
    },
    // completion token
    [this](const auto ec, auto) {
      lgr.debug("Response transfer stats: {}", reply_stats.str());
      if (ec)
        lgr.error("Error reading levels: {}", ec.message());
      stop(ec);
    });
}

template <typename D, typename L>
auto
client_connection<D, L>::stop(std::error_code ec) -> void {
  status = ec;
  (void)socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
  (void)socket.close(ec);
  watchdog_timer.cancel();
}

template <typename D, typename L>
auto
client_connection<D, L>::watchdog() -> void {
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

template <typename lvl>
class intervals_client : public client_connection<intervals_client<lvl>, lvl> {
public:
  intervals_client(const std::string &hostname, const std::string &port,
                   const request &req, const query_container &query) :
    base_t(hostname, port, req), query{query} {}

  // clang-format off
  intervals_client(const intervals_client &) = delete;
  auto operator=(const intervals_client &) -> intervals_client & = delete;
  intervals_client(intervals_client &&) = delete;
  auto operator=(intervals_client &&) -> intervals_client & = delete;
  ~intervals_client() = default;
  // clang-format on

  auto
  write_request_header() -> void {
    base_t::set_deadline(base_t::comm_timeout_sec);
    // clang-format off
    asio::async_write(base_t::socket, asio::buffer(base_t::req_buf),
      [this](const auto ec, auto) {
        if (ec)
          base_t::handle_write_failure(ec);
        else
          write_query();
      });
    // clang-format on
  }

private:
  auto
  write_query() -> void {
    base_t::set_deadline(base_t::comm_timeout_sec);
    asio::async_write(
      base_t::socket, asio::buffer(query.data(), query.n_bytes()),
      // completion condition
      [this](const auto ec, const auto n_bytes) -> std::size_t {
        auto completion_condition = asio::transfer_all();
        base_t::set_deadline(base_t::comm_timeout_sec);
        if (n_bytes > 0)
          query_stats.update(n_bytes);
        return base_t::is_stopped() ? 0 : completion_condition(ec, n_bytes);
      },
      // completion token
      [this](const auto ec, auto) {
        base_t::lgr.debug("Sent query ({})", query_stats.str());
        if (ec)
          base_t::handle_write_failure(ec);
        else
          base_t::read_response_header();
      });
  }

  using base_t = client_connection<intervals_client<lvl>, lvl>;
  const query_container &query;
  transfer_stats query_stats;
};  // class intervals_client

template <typename lvl>
class bins_client : public client_connection<bins_client<lvl>, lvl> {
public:
  bins_client(const std::string &hostname, const std::string &port,
              const request &req) : base_t(hostname, port, req) {}

  auto
  write_request_header() -> void {
    base_t::set_deadline(base_t::comm_timeout_sec);
    asio::async_write(base_t::socket, asio::buffer(base_t::req_buf),
                      [this](const auto ec, auto) {
                        if (ec)
                          base_t::handle_write_failure(ec);
                        else
                          base_t::read_response_header();
                      });
  }

private:
  using base_t = client_connection<bins_client<lvl>, lvl>;
};  // class bins_client

}  // namespace transferase

#endif  // LIB_CLIENT_CONNECTION_HPP_
