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

#ifndef LIB_CLIENT_CONNECTION_HPP_
#define LIB_CLIENT_CONNECTION_HPP_

#include "level_container_md.hpp"
#include "logger.hpp"
#include "query_container.hpp"
#include "request.hpp"
#include "response.hpp"

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
class client_connection_base {
public:
  client_connection_base(const std::string &hostname,
                         const std::string &port_number, const request &req);

  // allow const or ref data members (cppcoreguidelines)
  // clang-format off
  client_connection_base(const client_connection_base &) = delete;
  auto operator=(const client_connection_base &) -> client_connection_base & = delete;
  client_connection_base(client_connection_base &&) noexcept = delete;
  auto operator=(client_connection_base &&) noexcept -> client_connection_base & = delete;
  ~client_connection_base() = default;
  // clang-format on

  auto
  run() -> std::error_code {
    io_context.run();
    return status;
  }

  [[nodiscard]] auto
  get_levels() noexcept -> level_container_md<level_element> {
    return std::move(resp_container);
  }

  [[nodiscard]] auto
  take_levels() noexcept -> level_container_md<level_element> {
    return std::move(resp_container);
  }

  auto
  handle_write_request(const std::error_code ec) -> void;

private:
  auto
  handle_resolve(const std::error_code ec,
                 const asio::ip::tcp::resolver::results_type &endpoints)
    -> void;

  auto
  handle_connect(const std::error_code ec) -> void;

  auto
  handle_read_response_header(std::error_code ec) -> void;

  auto
  do_read_response_payload() -> void;

  auto
  handle_failure_explanation(const std::error_code ec) -> void;

  auto
  do_finish(const std::error_code ec) -> void;

  auto
  check_deadline() -> void;

  auto
  prepare_to_read_response_payload() -> void {
    // get space for query_container
    resp_container.resize(resp_hdr.rows, resp_hdr.cols);
    incoming_bytes_remaining = get_incoming_n_bytes();  // init counters
    incoming_bytes_received = 0;
  }

  auto
  get_incoming_n_bytes() const -> std::uint32_t {
    return sizeof(level_element) * resp_hdr.rows * resp_hdr.cols;
  }

  [[nodiscard]] auto
  get_incoming_data_buffer() noexcept -> char * {
    return resp_container.data();
  }

private:
  asio::io_context io_context;
  asio::ip::tcp::resolver resolver;

public:
  asio::ip::tcp::socket socket;
  asio::steady_timer deadline;
  request_buffer req_buf{};

private:
  request req{};

  response_header_buffer resp_hdr_buf{};
  response_header resp_hdr{};
  level_container_md<level_element> resp_container{};

  std::error_code status{};

public:
  logger &lgr;

private:
  /// This is the timeout for individual read operations, not the
  /// time to read entire messages
  std::chrono::seconds read_timeout_seconds{3};

  // ADS: this timeout applies when the server has received the
  // request (with query if applicable) and is doing the work.
  std::chrono::seconds wait_for_work_timeout_seconds{60};  // NOLINT

  // These help keep track of where we are in the incoming levels;
  // they might best be associated with the response.
  std::size_t incoming_bytes_received{};
  std::size_t incoming_bytes_remaining{};
};  // class client_connection_base

template <typename D, typename L>
client_connection_base<D, L>::client_connection_base(
  const std::string &hostname, const std::string &port_number,
  const request &req) :
  resolver(io_context), socket(io_context), deadline{socket.get_executor()},
  req{req}, lgr{logger::instance()} {
  // (1) call async, (2) set deadline, (3) register check_deadline
  const auto token = [this](const auto &error, const auto &results) {
    handle_resolve(error, results);
  };
  std::error_code ec;
  asio::ip::make_address(hostname, ec);
  if (ec) {  // hostname not an IP address
    lgr.debug("Resolving address for hostname: {}", hostname);
    resolver.async_resolve(hostname, port_number, token);
  }
  else {  // hostname given as IP address
    lgr.debug("Avoiding address resolution (ip: {})", hostname);
    const auto ns = asio::ip::resolver_query_base::numeric_service;
    resolver.async_resolve(hostname, port_number, ns, token);
  }
  deadline.expires_after(read_timeout_seconds);
  deadline.async_wait([this](auto) { check_deadline(); });
}

template <typename D, typename L>
auto
client_connection_base<D, L>::handle_resolve(
  const std::error_code ec,
  const asio::ip::tcp::resolver::results_type &endpoints) -> void {
  if (!ec) {
    asio::async_connect(socket, endpoints, [this](const auto &error, auto) {
      handle_connect(error);
    });
    deadline.expires_after(read_timeout_seconds);
  }
  else {
    lgr.debug("Error resolving server: {}", ec.message());
    do_finish(ec);
  }
}

template <typename D, typename L>
auto
client_connection_base<D, L>::handle_connect(std::error_code ec) -> void {
  deadline.expires_at(asio::steady_timer::time_point::max());
  if (!ec) {
    lgr.debug("Connected to server: {}",
              (std::ostringstream() << socket.remote_endpoint()).str());
    ec = compose(req_buf, req);
    if (!ec) {
      static_cast<D *>(this)->handle_connect_impl();
      deadline.expires_after(read_timeout_seconds);
    }
    else {
      lgr.debug("Error forming request: {}", ec.message());
      do_finish(ec);
    }
  }
  else {
    lgr.debug("Error connecting: {}", ec.message());
    do_finish(ec);
  }
}

template <typename D, typename L>
auto
client_connection_base<D, L>::handle_write_request(const std::error_code ec)
  -> void {
  deadline.expires_at(asio::steady_timer::time_point::max());
  if (!ec) {
    asio::async_read(
      socket, asio::buffer(resp_hdr_buf),
      asio::transfer_exactly(response_header_buffer_size),
      [this](const auto error, auto) { handle_read_response_header(error); });
    deadline.expires_after(wait_for_work_timeout_seconds);
  }
  else {
    lgr.debug("Error writing request: {}", ec.message());
    deadline.expires_after(read_timeout_seconds);
    // wait for an explanation of the problem
    asio::async_read(
      socket, asio::buffer(resp_hdr_buf),
      asio::transfer_exactly(response_header_buffer_size),
      [this](const auto error, auto) { handle_failure_explanation(error); });
  }
}

template <typename D, typename L>
auto
client_connection_base<D, L>::handle_read_response_header(std::error_code ec)
  -> void {
  // ADS: does this go here?
  deadline.expires_at(asio::steady_timer::time_point::max());
  if (!ec) {
    ec = parse(resp_hdr_buf, resp_hdr);
    if (!ec) {
      lgr.debug("Response header: {}", resp_hdr.summary());
      if (resp_hdr.status)
        do_finish(resp_hdr.status);
      else {
        prepare_to_read_response_payload();
        do_read_response_payload();
      }
    }
    else {
      lgr.debug("Error: {}", ec.message());
      do_finish(ec);
    }
  }
  else {
    lgr.debug("Error reading response header: {}", ec.message());
    do_finish(ec);
  }
}

template <typename D, typename L>
auto
client_connection_base<D, L>::do_read_response_payload() -> void {
  socket.async_read_some(
    asio::buffer(get_incoming_data_buffer() + incoming_bytes_received,
                 incoming_bytes_remaining),
    [this](const std::error_code ec, const std::size_t bytes_transferred) {
      deadline.expires_at(asio::steady_timer::time_point::max());
      if (!ec) {
        incoming_bytes_remaining -= bytes_transferred;
        incoming_bytes_received += bytes_transferred;
        if (incoming_bytes_remaining == 0) {
          do_finish(ec);
        }
        else {
          do_read_response_payload();
        }
      }
      else {
        lgr.error("Error reading levels: {}", ec.message());
        // exiting the read loop -- no deadline for now
        do_finish(ec);
      }
    });
  deadline.expires_after(read_timeout_seconds);
}

template <typename D, typename L>
auto
client_connection_base<D, L>::handle_failure_explanation(std::error_code ec)
  -> void {
  deadline.expires_at(asio::steady_timer::time_point::max());
  if (!ec) {
    ec = parse(resp_hdr_buf, resp_hdr);
    if (!ec) {
      lgr.debug("Response header: {}", resp_hdr.summary());
      do_finish(resp_hdr.status);
    }
    else {
      lgr.debug("Error: {}", ec.message());
      do_finish(ec);
    }
  }
  else {
    lgr.debug("Error reading response header: {}", ec.message());
    do_finish(ec);
  }
}

template <typename D, typename L>
auto
client_connection_base<D, L>::do_finish(const std::error_code ec) -> void {
  // same consequence as canceling
  deadline.expires_at(asio::steady_timer::time_point::max());
  status = ec;
  std::error_code shutdown_ec;  // for non-throwing
  // nothing actually returned below
  (void)socket.shutdown(asio::ip::tcp::socket::shutdown_both, shutdown_ec);
  std::error_code socket_close_ec;  // for non-throwing
  // nothing actually returned below
  (void)socket.close(socket_close_ec);
}

template <typename D, typename L>
auto
client_connection_base<D, L>::check_deadline() -> void {
  if (!socket.is_open())  // ADS: when can this happen?
    return;

  if (const auto right_now = asio::steady_timer::clock_type::now();
      deadline.expiry() <= right_now) {
    // deadline passed: close socket so remaining async ops are
    // cancelled (see below)
    const auto delta = std::chrono::duration_cast<std::chrono::seconds>(
      right_now - deadline.expiry());
    lgr.debug("Error deadline expired by: {}", delta.count());

    std::error_code shutdown_ec;  // for non-throwing
    // nothing actually returned below
    (void)socket.shutdown(asio::ip::tcp::socket::shutdown_both, shutdown_ec);
    deadline.expires_at(asio::steady_timer::time_point::max());

    /* ADS: closing here if needed?? */
    std::error_code socket_close_ec;  // for non-throwing
    // nothing actually returned below
    (void)socket.close(socket_close_ec);
  }

  // wait again
  deadline.async_wait([this](auto) { check_deadline(); });
}

template <typename lvl>
class intervals_client_connection
  : public client_connection_base<intervals_client_connection<lvl>, lvl> {
public:
  intervals_client_connection(const std::string &hostname,
                              const std::string &port_number,
                              const request &req,
                              const query_container &query) :
    base_class_t(hostname, port_number, req), query{query} {}

  // allow const or ref data members (cppcoreguidelines)
  intervals_client_connection(const intervals_client_connection &) = delete;
  auto
  operator=(const intervals_client_connection &)
    -> intervals_client_connection & = delete;
  intervals_client_connection(intervals_client_connection &&) noexcept = delete;
  auto
  operator=(intervals_client_connection &&) noexcept
    -> intervals_client_connection & = delete;
  ~intervals_client_connection() = default;

  auto
  handle_connect_impl() noexcept -> void {
    asio::async_write(base_class_t::socket, asio::buffer(base_class_t::req_buf),
                      [this](const auto error, auto) {
                        if (!error) {
                          // prepare to write query payload
                          bytes_remaining =
                            query.get_n_bytes();  // init counters
                          bytes_sent = 0;
                          write_query();
                        }
                        else {
                          base_class_t::handle_write_request(error);
                        }
                      });
  }

  auto
  write_query() noexcept -> void {
    base_class_t::socket.async_write_some(
      // NOLINTNEXTLINE(*-pointer-arithmetic)
      asio::buffer(query.data() + bytes_sent, bytes_remaining),
      [this](const auto error, const std::size_t bytes_transferred) {
        base_class_t::deadline.expires_at(
          asio::steady_timer::time_point::max());
        if (!error) {
          bytes_remaining -= bytes_transferred;
          bytes_sent += bytes_transferred;

          // ADS: collect the stats here; should be refactored
          ++n_writes;
          max_write_size = std::max(max_write_size, bytes_transferred);
          min_write_size = std::min(min_write_size, bytes_transferred);
          if (bytes_remaining == 0) {
            base_class_t::lgr.debug(
              "Sent query ({}B, writes={}, max={}B, min={}B, mean={}B)",
              bytes_sent, n_writes, max_write_size, min_write_size,
              bytes_sent / n_writes);
            base_class_t::handle_write_request(error);
          }
          else {
            write_query();
          }
        }
        else {
          base_class_t::handle_write_request(error);
        }
      });
    base_class_t::deadline.expires_after(write_timeout_seconds);
  }

private:
  using base_class_t =
    client_connection_base<intervals_client_connection<lvl>, lvl>;

  const query_container &query;
  std::chrono::seconds write_timeout_seconds{3};

  // ADS: vars to keep track of what has been sent and what remains
  std::size_t bytes_sent{};
  std::size_t bytes_remaining{};

  // ADS: vars to collect stats on sizes of individual write operations
  std::size_t n_writes{};
  std::size_t min_write_size{std::numeric_limits<std::size_t>::max()};
  std::size_t max_write_size{0};
};  // class intervals_client_connection

template <typename lvl>
class bins_client_connection
  : public client_connection_base<bins_client_connection<lvl>, lvl> {
public:
  bins_client_connection(const std::string &hostname,
                         const std::string &port_number, const request &req) :
    base_class_t(hostname, port_number, req) {}

  auto
  handle_connect_impl() noexcept {
    asio::async_write(base_class_t::socket, asio::buffer(base_class_t::req_buf),
                      [this](const auto error, auto) {
                        base_class_t::handle_write_request(error);
                      });
  }

private:
  using base_class_t = client_connection_base<bins_client_connection<lvl>, lvl>;
};  // class bins_client_connection

}  // namespace transferase

#endif  // LIB_CLIENT_CONNECTION_HPP_
