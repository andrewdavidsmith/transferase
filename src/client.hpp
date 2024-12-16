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

#include "logger.hpp"
#include "request.hpp"
#include "response.hpp"

#include <chrono>
#include <compare>      // for operator<=, strong_ordering
#include <cstddef>      // for size_t
#include <type_traits>  // for is_same

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/system.hpp>  // for boost::system::error_code

#include <cstdint>  // std::uint32_t
#include <string>
#include <system_error>
#include <utility>  // std::swap std::move
#include <vector>

namespace xfrase {
template <typename counts_type, typename req_type> class client {
public:
  client(const std::string &server, const std::string &port,
         request_header &req_hdr, req_type &req);

  auto
  run() -> std::error_code {
    io_context.run();
    return status;
  }

  auto
  get_counts() const -> const std::vector<counts_type> & {
    return resp.counts;
  }

  auto
  take_counts() -> std::vector<counts_type> {
    // ADS: this function resets resp.counts and avoids copy
    std::vector<counts_type> moved_out;
    std::swap(moved_out, resp.counts);
    return moved_out;
  }

private:
  auto
  handle_resolve(const std::error_code &err,
                 const boost::asio::ip::tcp::resolver::results_type &endpoints);
  auto
  handle_connect(const std::error_code &err);
  auto
  handle_write_request(const std::error_code &err);
  auto
  handle_read_response_header(const std::error_code &err);
  auto
  do_read_counts() -> void;
  auto
  handle_failure_explanation(const std::error_code &err);
  auto
  do_finish(const std::error_code &err);
  auto
  check_deadline();

  auto
  prepare_to_read_counts() -> void;
  auto
  get_counts_n_bytes() const -> std::uint32_t;
  auto
  resp_get_counts_data() -> char * {
    return reinterpret_cast<char *>(resp.counts.data());
  }

  boost::asio::io_context io_context;
  boost::asio::ip::tcp::resolver resolver;
  boost::asio::ip::tcp::socket socket;
  boost::asio::steady_timer deadline;

  request_header_buffer req_hdr_buf;
  request_header req_hdr;
  req_type req;

  response_header_buffer resp_hdr_buf;
  response_header resp_hdr;
  response<counts_type> resp;

  std::error_code status;
  logger &lgr;
  std::chrono::seconds read_timeout_seconds{3};

  // These help keep track of where we are in the incoming counts;
  // they might best be associated with the response.
  std::size_t counts_byte{};
  std::size_t counts_remaining{};
};  // class client

template <typename counts_type, typename req_type>
client<counts_type, req_type>::client(const std::string &server,
                                      const std::string &port,
                                      request_header &req_hdr, req_type &req) :
  resolver(io_context), socket(io_context), deadline{socket.get_executor()},
  req_hdr{req_hdr}, req{std::move(req)},  // move b/c req can be big
  lgr{logger::instance()} {
  // (1) call async, (2) set deadline, (3) register check_deadline
  const auto token = [this](const auto &error, const auto &results) {
    handle_resolve(error, results);
  };
  boost::system::error_code ec;
  boost::asio::ip::address addr{};
  addr.from_string(server, ec);
  if (ec) {  // server not an IP address
    lgr.debug("Resolving address for hostname: {}", server);
    resolver.async_resolve(server, port, token);
  }
  else {  // server given as IP address
    lgr.debug("Avoiding address resolution (ip: {})", server);
    const auto ns = boost::asio::ip::resolver_query_base::numeric_service;
    resolver.async_resolve(server, port, ns, token);
  }
  deadline.expires_after(read_timeout_seconds);
  deadline.async_wait([this](auto) { check_deadline(); });
}

template <typename counts_type, typename req_type>
auto
client<counts_type, req_type>::handle_resolve(
  const std::error_code &err,
  const boost::asio::ip::tcp::resolver::results_type &endpoints) {
  if (!err) {
    boost::asio::async_connect(
      socket, endpoints,
      [this](const auto &error, auto) { handle_connect(error); });
    deadline.expires_after(read_timeout_seconds);
  }
  else {
    lgr.debug("Error resolving server: {}", err);
    do_finish(err);
  }
}

template <typename counts_type, typename req_type>
auto
client<counts_type, req_type>::handle_connect(const std::error_code &err) {
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  if (!err) {
    lgr.debug("Connected to server: {}",
              boost::lexical_cast<std::string>(socket.remote_endpoint()));
    if (const auto req_hdr_compose{compose(req_hdr_buf, req_hdr)};
        !req_hdr_compose.error) {
      if (const auto req_body_compose =
            compose(req_hdr_compose.ptr,
                    req_hdr_buf.data() + request_header_buf_size, req);
          !req_body_compose.error) {
        if constexpr (std::is_same<req_type, request>::value) {
          boost::asio::async_write(
            socket,
            std::vector<boost::asio::const_buffer>{
              boost::asio::buffer(req_hdr_buf),
              boost::asio::buffer(req.offsets),
            },
            [this](auto error, auto) { this->handle_write_request(error); });
        }
        else {
          boost::asio::async_write(
            socket, boost::asio::buffer(req_hdr_buf),
            [this](auto error, auto) { this->handle_write_request(error); });
        }
        deadline.expires_after(read_timeout_seconds);
      }
      else {
        lgr.debug("Error forming request body: {}", req_body_compose.error);
        do_finish(req_body_compose.error);
      }
    }
    else {
      lgr.debug("Error forming request header: {}", req_hdr_compose.error);
      do_finish(req_hdr_compose.error);
    }
  }
  else {
    lgr.debug("Error connecting: {}", err);
    do_finish(err);
  }
}

template <typename counts_type, typename req_type>
auto
client<counts_type, req_type>::handle_write_request(
  const std::error_code &err) {
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  if (!err) {
    boost::asio::async_read(
      socket, boost::asio::buffer(resp_hdr_buf),
      boost::asio::transfer_exactly(response_buf_size),
      [this](const auto error, auto) { handle_read_response_header(error); });
    deadline.expires_after(read_timeout_seconds);
  }
  else {
    lgr.debug("Error writing request: {}", err);
    deadline.expires_after(read_timeout_seconds);
    // wait for an explanation of the problem
    boost::asio::async_read(
      socket, boost::asio::buffer(resp_hdr_buf),
      boost::asio::transfer_exactly(response_buf_size),
      [this](const auto error, auto) { handle_failure_explanation(error); });
  }
}

template <typename counts_type, typename req_type>
auto
client<counts_type, req_type>::get_counts_n_bytes() const -> std::uint32_t {
  return sizeof(counts_type) * resp_hdr.response_size;
}

template <typename counts_type, typename req_type>
auto
client<counts_type, req_type>::prepare_to_read_counts() -> void {
  // This function is needed because this can't be done in the
  // read_offsets() function as it is recursive
  resp.counts.resize(resp_hdr.response_size);  // get space for offsets
  counts_remaining = get_counts_n_bytes();     // init counters
  counts_byte = 0;                             // should be init to this
}

template <typename counts_type, typename req_type>
auto
client<counts_type, req_type>::handle_read_response_header(
  const std::error_code &err) {
  // ADS: does this go here?
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  if (!err) {
    if (const auto resp_hdr_parse{parse(resp_hdr_buf, resp_hdr)};
        !resp_hdr_parse.error) {
      lgr.debug("Response header: {}", resp_hdr.summary());
      if (resp_hdr.status)
        do_finish(resp_hdr.status);
      else {
        prepare_to_read_counts();
        do_read_counts();
      }
    }
    else {
      lgr.debug("Error: {}", resp_hdr_parse.error);
      do_finish(resp_hdr_parse.error);
    }
  }
  else {
    lgr.debug("Error reading response header: {}", err);
    do_finish(err);
  }
}

template <typename counts_type, typename req_type>
auto
client<counts_type, req_type>::do_read_counts() -> void {
  socket.async_read_some(
    boost::asio::buffer(resp_get_counts_data() + counts_byte, counts_remaining),
    [this](const boost::system::error_code ec,
           const std::size_t bytes_transferred) {
      deadline.expires_at(boost::asio::steady_timer::time_point::max());
      if (!ec) {
        counts_remaining -= bytes_transferred;
        counts_byte += bytes_transferred;
        if (counts_remaining == 0) {
          do_finish(ec);
        }
        else {
          do_read_counts();
        }
      }
      else {
        lgr.error("Error reading counts: {}", ec);
        // exiting the read loop -- no deadline for now
        do_finish(ec);
      }
    });
  deadline.expires_after(read_timeout_seconds);
}

template <typename counts_type, typename req_type>
auto
client<counts_type, req_type>::handle_failure_explanation(
  const std::error_code &err) {
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  if (!err) {
    if (const auto resp_hdr_parse{parse(resp_hdr_buf, resp_hdr)};
        !resp_hdr_parse.error) {
      lgr.debug("Response header: {}", resp_hdr.summary());
      do_finish(resp_hdr.status);
    }
    else {
      lgr.debug("Error: {}", resp_hdr_parse.error);
      do_finish(resp_hdr_parse.error);
    }
  }
  else {
    lgr.debug("Error reading response header: {}", err);
    do_finish(err);
  }
}

template <typename counts_type, typename req_type>
auto
client<counts_type, req_type>::do_finish(const std::error_code &err) {
  // same consequence as canceling
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  status = err;
  boost::system::error_code shutdown_ec;  // for non-throwing
  socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, shutdown_ec);
  boost::system::error_code socket_close_ec;  // for non-throwing
  socket.close(socket_close_ec);
}

template <typename counts_type, typename req_type>
auto
client<counts_type, req_type>::check_deadline() {
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
  deadline.async_wait([this](auto) { this->check_deadline(); });
}
}  // namespace xfrase

#endif  // SRC_CLIENT_HPP_
