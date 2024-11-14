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
#include "request_handler.hpp"
#include "response.hpp"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>

#include <cstdint>  // std::uint32_t
#include <string>
#include <system_error>
#include <utility>  // std::swap std::move
#include <vector>

template <typename counts_type = counts_res_cov> class mxe_client {
public:
  mxe_client(const std::string &server, const std::string &port,
              request_header &req_hdr, request &req, logger &lgr);

  auto run() -> std::error_code {
    io_context.run();
    return status;
  }

  auto get_counts() const -> const std::vector<counts_type> & {
    return resp.counts;
  }

  auto take_counts() -> std::vector<counts_type> {
    // ADS: this function resets resp.counts and avoids copy
    std::vector<counts_type> moved_out;
    std::swap(moved_out, resp.counts);
    return moved_out;
  }

private:
  auto
  handle_resolve(const std::error_code &err,
                 const boost::asio::ip::tcp::resolver::results_type &endpoints);
  auto handle_connect(const std::error_code &err);
  auto handle_write_request(const std::error_code &err);
  auto handle_read_response_header(const std::error_code &err);
  auto do_read_counts();
  auto handle_failure_explanation(const std::error_code &err);
  auto do_finish(const std::error_code &err);
  auto check_deadline();

  boost::asio::io_context io_context;
  boost::asio::ip::tcp::resolver resolver;
  boost::asio::ip::tcp::socket socket;
  boost::asio::steady_timer deadline;

  request_buffer req_buf;
  request_header req_hdr;
  request req;

  response_buffer resp_buf;
  response_header resp_hdr;
  response<counts_type> resp;

  std::error_code status;
  logger &lgr;
  std::chrono::seconds read_timeout_seconds{3};
};  // class mxe_client

template <typename counts_type>
mxe_client<counts_type>::mxe_client(const std::string &server,
                                      const std::string &port,
                                      request_header &req_hdr, request &req,
                                      logger &lgr) :
  resolver(io_context), socket(io_context), deadline{socket.get_executor()},
  req_hdr{req_hdr}, req{std::move(req)},  // move b/c req can be big
  lgr{lgr} {
  // (1) call async, (2) set deadline, (3) register check_deadline
  const auto handle_resolve_token = [this](const auto &error, auto results) {
    this->handle_resolve(error, results);
  };
  boost::system::error_code ec;
  boost::asio::ip::address addr{};
  addr.from_string(server, ec);
  if (ec) {  // server not an IP address
    lgr.debug("Resolving address for hostname: {}", server);
    resolver.async_resolve(server, port, handle_resolve_token);
  }
  else {  // server given as IP address
    lgr.debug("Avoiding address resolution (ip: {})", server);
    const auto ns = boost::asio::ip::resolver_query_base::numeric_service;
    resolver.async_resolve(server, port, ns, handle_resolve_token);
  }
  deadline.expires_after(read_timeout_seconds);
  deadline.async_wait([this](auto) { this->check_deadline(); });
}

template <typename counts_type>
auto
mxe_client<counts_type>::handle_resolve(
  const std::error_code &err,
  const boost::asio::ip::tcp::resolver::results_type &endpoints) {
  if (!err) {
    boost::asio::async_connect(
      socket, endpoints,
      [this](const auto &error, auto) { this->handle_connect(error); });
    deadline.expires_after(read_timeout_seconds);
  }
  else {
    lgr.debug("Error resolving server: {}", err);
    do_finish(err);
  }
}

template <typename counts_type>
auto
mxe_client<counts_type>::handle_connect(const std::error_code &err) {
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  if (!err) {
    lgr.debug("Connected to server: {}",
              boost::lexical_cast<std::string>(socket.remote_endpoint()));
    if (const auto req_hdr_compose{compose(req_buf, req_hdr)};
        !req_hdr_compose.error) {
      if (const auto req_body_compose = to_chars(
            req_hdr_compose.ptr, req_buf.data() + request_buf_size, req);
          !req_body_compose.error) {
        boost::asio::async_write(
          socket,
          std::vector<boost::asio::const_buffer>{
            boost::asio::buffer(req_buf),
            boost::asio::buffer(req.offsets),
          },
          [this](auto error, auto) { this->handle_write_request(error); });
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

template <typename counts_type>
auto
mxe_client<counts_type>::handle_write_request(const std::error_code &err) {
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  if (!err) {
    boost::asio::async_read(
      socket, boost::asio::buffer(resp_buf),
      boost::asio::transfer_exactly(response_buf_size),
      [this](auto error, auto) { this->handle_read_response_header(error); });
    deadline.expires_after(read_timeout_seconds);
  }
  else {
    lgr.debug("Error writing request: {}", err);
    deadline.expires_after(read_timeout_seconds);
    // wait for an explanation of the problem
    boost::asio::async_read(
      socket, boost::asio::buffer(resp_buf),
      boost::asio::transfer_exactly(response_buf_size),
      [this](auto error, auto) { this->handle_failure_explanation(error); });
  }
}

template <typename counts_type>
auto
mxe_client<counts_type>::handle_read_response_header(
  const std::error_code &err) {
  // ADS: does this go here?
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  if (!err) {
    if (const auto resp_hdr_parse{parse(resp_buf, resp_hdr)};
        !resp_hdr_parse.error) {
      lgr.debug("Response header: {}", resp_hdr.summary());
      do_read_counts();
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

template <typename counts_type>
auto
mxe_client<counts_type>::handle_failure_explanation(
  const std::error_code &err) {
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  if (!err) {
    if (const auto resp_hdr_parse{parse(resp_buf, resp_hdr)};
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

template <typename counts_type>
auto
mxe_client<counts_type>::do_read_counts() {
  // ADS: this 'resize' seems to belong with the response class
  resp.counts.resize(req.n_intervals);
  boost::asio::async_read(
    socket, boost::asio::buffer(resp.counts),
    boost::asio::transfer_exactly(resp.get_counts_n_bytes()),
    [this](const auto &error, auto) { this->do_finish(error); });
  deadline.expires_after(read_timeout_seconds);
}

template <typename counts_type>
auto
mxe_client<counts_type>::do_finish(const std::error_code &err) {
  // same consequence as canceling
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  status = err;
  boost::system::error_code shutdown_ec;  // for non-throwing
  socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, shutdown_ec);
  boost::system::error_code socket_close_ec;  // for non-throwing
  socket.close(socket_close_ec);
}

template <typename counts_type>
auto
mxe_client<counts_type>::check_deadline() {
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

#endif  // SRC_CLIENT_HPP_
