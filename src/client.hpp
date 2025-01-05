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

#ifndef SRC_CLIENT_HPP_
#define SRC_CLIENT_HPP_

#include "level_container.hpp"
#include "logger.hpp"
#include "query_container.hpp"
#include "request.hpp"
#include "response.hpp"

#include <chrono>
#include <compare>  // for std::strong_ordering
#include <cstddef>  // for std::size_t

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/system.hpp>  // for boost::system::error_code

#include <cstdint>  // std::uint32_t
#include <string>
#include <system_error>
#include <utility>  // std::swap std::move
#include <vector>

namespace transferase {

template <typename derived, typename level_element> class client_base {
public:
  client_base(const std::string &hostname, const std::string &port_number,
              const request &req);

  auto
  run() -> std::error_code {
    io_context.run();
    return status;
  }

  auto
  get_levels() const -> const level_container<level_element> & {
    return resp.levels;
  }

  auto
  take_levels() -> level_container<level_element> && {
    return std::move(resp.levels);
  }

protected:
  auto
  handle_write_request(const std::error_code ec) -> void;

private:
  auto
  handle_resolve(const std::error_code ec,
                 const boost::asio::ip::tcp::resolver::results_type &endpoints)
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
    resp.levels.resize(resp_hdr.response_size);
    levels_bytes_remaining = get_levels_n_bytes();  // init counters
    levels_bytes_received = 0;
  }

  auto
  get_levels_n_bytes() const -> std::uint32_t {
    return sizeof(level_element) * resp_hdr.response_size;
  }

  auto
  resp_get_levels_data() -> char * {
    return reinterpret_cast<char *>(resp.levels.data());
  }

private:
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::resolver resolver;

protected:
  boost::asio::ip::tcp::socket socket;

private:
  boost::asio::steady_timer deadline;

protected:
  request_buffer req_buf{};

private:
  request req{};

  response_header_buffer resp_hdr_buf{};
  response_header resp_hdr{};
  response<level_element> resp{};

  std::error_code status{};
  logger &lgr;
  /// This is the timeout for individual read operations, not the
  /// time to read entire messages
  std::chrono::seconds read_timeout_seconds{3};

  // These help keep track of where we are in the incoming levels;
  // they might best be associated with the response.
  std::size_t levels_bytes_received{};
  std::size_t levels_bytes_remaining{};
};  // class client_base

template <typename D, typename L>
client_base<D, L>::client_base(const std::string &hostname,
                               const std::string &port_number,
                               const request &req) :
  resolver(io_context), socket(io_context), deadline{socket.get_executor()},
  req{req}, lgr{logger::instance()} {
  // (1) call async, (2) set deadline, (3) register check_deadline
  const auto token = [this](const auto &error, const auto &results) {
    handle_resolve(error, results);
  };
  boost::system::error_code ec;
  boost::asio::ip::address addr{};
  addr.from_string(hostname, ec);
  if (ec) {  // hostname not an IP address
    lgr.debug("Resolving address for hostname: {}", hostname);
    resolver.async_resolve(hostname, port_number, token);
  }
  else {  // hostname given as IP address
    lgr.debug("Avoiding address resolution (ip: {})", hostname);
    const auto ns = boost::asio::ip::resolver_query_base::numeric_service;
    resolver.async_resolve(hostname, port_number, ns, token);
  }
  deadline.expires_after(read_timeout_seconds);
  deadline.async_wait([this](auto) { check_deadline(); });
}

template <typename D, typename L>
auto
client_base<D, L>::handle_resolve(
  const std::error_code ec,
  const boost::asio::ip::tcp::resolver::results_type &endpoints) -> void {
  if (!ec) {
    boost::asio::async_connect(
      socket, endpoints,
      [this](const auto &error, auto) { handle_connect(error); });
    deadline.expires_after(read_timeout_seconds);
  }
  else {
    lgr.debug("Error resolving server: {}", ec);
    do_finish(ec);
  }
}

template <typename D, typename L>
auto
client_base<D, L>::handle_connect(std::error_code ec) -> void {
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  if (!ec) {
    lgr.debug("Connected to server: {}",
              boost::lexical_cast<std::string>(socket.remote_endpoint()));
    ec = compose(req_buf, req);
    if (!ec) {
      static_cast<D *>(this)->handle_connect_impl();
      deadline.expires_after(read_timeout_seconds);
    }
    else {
      lgr.debug("Error forming request: {}", ec);
      do_finish(ec);
    }
  }
  else {
    lgr.debug("Error connecting: {}", ec);
    do_finish(ec);
  }
}

template <typename D, typename L>
auto
client_base<D, L>::handle_write_request(const std::error_code ec) -> void {
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  if (!ec) {
    boost::asio::async_read(
      socket, boost::asio::buffer(resp_hdr_buf),
      boost::asio::transfer_exactly(response_header_buffer_size),
      [this](const auto error, auto) { handle_read_response_header(error); });
    deadline.expires_after(read_timeout_seconds);
  }
  else {
    lgr.debug("Error writing request: {}", ec);
    deadline.expires_after(read_timeout_seconds);
    // wait for an explanation of the problem
    boost::asio::async_read(
      socket, boost::asio::buffer(resp_hdr_buf),
      boost::asio::transfer_exactly(response_header_buffer_size),
      [this](const auto error, auto) { handle_failure_explanation(error); });
  }
}

template <typename D, typename L>
auto
client_base<D, L>::handle_read_response_header(std::error_code ec) -> void {
  // ADS: does this go here?
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
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
      lgr.debug("Error: {}", ec);
      do_finish(ec);
    }
  }
  else {
    lgr.debug("Error reading response header: {}", ec);
    do_finish(ec);
  }
}

template <typename D, typename L>
auto
client_base<D, L>::do_read_response_payload() -> void {
  socket.async_read_some(
    boost::asio::buffer(resp_get_levels_data() + levels_bytes_received,
                        levels_bytes_remaining),
    [this](const boost::system::error_code ec,
           const std::size_t bytes_transferred) {
      deadline.expires_at(boost::asio::steady_timer::time_point::max());
      if (!ec) {
        levels_bytes_remaining -= bytes_transferred;
        levels_bytes_received += bytes_transferred;
        if (levels_bytes_remaining == 0) {
          do_finish(ec);
        }
        else {
          do_read_response_payload();
        }
      }
      else {
        lgr.error("Error reading levels: {}", ec);
        // exiting the read loop -- no deadline for now
        do_finish(ec);
      }
    });
  deadline.expires_after(read_timeout_seconds);
}

template <typename D, typename L>
auto
client_base<D, L>::handle_failure_explanation(std::error_code ec) -> void {
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  if (!ec) {
    ec = parse(resp_hdr_buf, resp_hdr);
    if (!ec) {
      lgr.debug("Response header: {}", resp_hdr.summary());
      do_finish(resp_hdr.status);
    }
    else {
      lgr.debug("Error: {}", ec);
      do_finish(ec);
    }
  }
  else {
    lgr.debug("Error reading response header: {}", ec);
    do_finish(ec);
  }
}

template <typename D, typename L>
auto
client_base<D, L>::do_finish(const std::error_code ec) -> void {
  // same consequence as canceling
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  status = ec;
  boost::system::error_code shutdown_ec;  // for non-throwing
  socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, shutdown_ec);
  boost::system::error_code socket_close_ec;  // for non-throwing
  socket.close(socket_close_ec);
}

template <typename D, typename L>
auto
client_base<D, L>::check_deadline() -> void {
  if (!socket.is_open())  // ADS: when can this happen?
    return;

  if (const auto right_now = boost::asio::steady_timer::clock_type::now();
      deadline.expiry() <= right_now) {
    // deadline passed: close socket so remaining async ops are
    // cancelled (see below)
    const auto delta = std::chrono::duration_cast<std::chrono::seconds>(
      right_now - deadline.expiry());
    lgr.debug("Error deadline expired by: {}", delta.count());

    boost::system::error_code shutdown_ec;  // for non-throwing
    socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, shutdown_ec);
    deadline.expires_at(boost::asio::steady_timer::time_point::max());

    /* ADS: closing here if needed?? */
    boost::system::error_code socket_close_ec;  // for non-throwing
    socket.close(socket_close_ec);
  }

  // wait again
  deadline.async_wait([this](auto) { check_deadline(); });
}

template <typename lvl>
class intervals_client : public client_base<intervals_client<lvl>, lvl> {
public:
  intervals_client(const std::string &hostname, const std::string &port_number,
                   const request &req, const query_container &query) :
    base_class_t(hostname, port_number, req), query{query} {}

  auto
  handle_connect_impl() {
    boost::asio::async_write(
      base_class_t::socket,
      std::vector<boost::asio::const_buffer>{
        boost::asio::buffer(base_class_t::req_buf),
        boost::asio::buffer(query.v),
      },
      [this](auto error, auto) { base_class_t::handle_write_request(error); });
  }

private:
  using base_class_t = client_base<intervals_client<lvl>, lvl>;

  const query_container &query;
};  // class intervals_client

template <typename lvl>
class bins_client : public client_base<bins_client<lvl>, lvl> {
public:
  bins_client(const std::string &hostname, const std::string &port_number,
              const request &req) : base_class_t(hostname, port_number, req) {}

  auto
  handle_connect_impl() {
    boost::asio::async_write(
      base_class_t::socket, boost::asio::buffer(base_class_t::req_buf),
      [this](auto error, auto) { base_class_t::handle_write_request(error); });
  }

private:
  using base_class_t = client_base<bins_client<lvl>, lvl>;
};  // class bins_client

}  // namespace transferase

#endif  // SRC_CLIENT_HPP_
