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

#include "client.hpp"

#include "logger.hpp"
#include "request_handler.hpp"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include <cstdint>
#include <string>
#include <utility>  // std::move
#include <vector>

using std::string;
using std::uint32_t;
using std::vector;

namespace chrono = std::chrono;
namespace asio = boost::asio;
namespace ph = boost::asio::placeholders;
namespace bs = boost::system;
using tcp = boost::asio::ip::tcp;
using steady_timer = boost::asio::steady_timer;

mc16_client::mc16_client(const string &server, const string &port,
                         request_header &req_hdr, request &req, logger &lgr) :
  resolver(io_context), socket(io_context), deadline{socket.get_executor()},
  req_hdr{req_hdr}, req{std::move(req)},  // move b/c req can be big
  lgr{lgr} {
  // (1) call async, (2) set deadline, (3) register check_deadline
  bs::error_code ec;
  asio::ip::address addr{};
  addr.from_string(server, ec);
  if (ec) {  // not an IP address
    lgr.debug("Resolving address for hostname: {}", server);
    resolver.async_resolve(
      server, port,
      std::bind(&mc16_client::handle_resolve, this, ph::error, ph::results));
  }
  else {
    lgr.debug("Avoiding address resolution (ip: {})", server);
    resolver.async_resolve(
      server, port, asio::ip::resolver_query_base::numeric_service,
      std::bind(&mc16_client::handle_resolve, this, ph::error, ph::results));
  }
  // tcp::resolver::query query(
  //   host, PORT, boost::asio::ip::resolver_query_base::numeric_service);
  [[maybe_unused]] const auto n_cancels =
    deadline.expires_after(chrono::seconds(read_timeout_seconds));
  deadline.async_wait(std::bind(&mc16_client::check_deadline, this));
}

auto
mc16_client::handle_resolve(const bs::error_code &err,
                            const tcp::resolver::results_type &endpoints)
  -> void {
  if (!err) {
    asio::async_connect(
      socket, endpoints,
      std::bind(&mc16_client::handle_connect, this, ph::error));
    // ADS: set deadline after calling async op
    [[maybe_unused]] const auto n_cancels =
      deadline.expires_after(chrono::seconds(read_timeout_seconds));
  }
  else {
    lgr.debug("Error resolving server: {}", err);
    do_finish(err);
  }
}

void
mc16_client::handle_connect(const bs::error_code &err) {
  [[maybe_unused]] const auto n_cancels_removing =
    deadline.expires_at(steady_timer::time_point::max());
  if (!err) {
    lgr.debug("Connected to server: {}",
              boost::lexical_cast<string>(socket.remote_endpoint()));
    if (const auto req_hdr_compose{compose(req_buf, req_hdr)};
        !req_hdr_compose.error) {
      if (const auto req_body_compose = to_chars(
            req_hdr_compose.ptr, req_buf.data() + request_buf_size, req);
          !req_body_compose.error) {
        asio::async_write(
          socket,
          std::vector<asio::const_buffer>{
            asio::buffer(req_buf),
            asio::buffer(req.offsets),
          },
          std::bind(&mc16_client::handle_write_request, this, ph::error));
        [[maybe_unused]] const auto n_cancels =
          deadline.expires_after(chrono::seconds(read_timeout_seconds));
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

auto
mc16_client::handle_write_request(const bs::error_code &err) -> void {
  [[maybe_unused]] const auto n_cancels_removing =
    deadline.expires_at(steady_timer::time_point::max());
  if (!err) {
    asio::async_read(
      socket, asio::buffer(resp_buf), asio::transfer_exactly(response_buf_size),
      std::bind(&mc16_client::handle_read_response_header, this, ph::error));
    [[maybe_unused]] const auto n_cancels =
      deadline.expires_after(chrono::seconds(read_timeout_seconds));
  }
  else {
    lgr.debug("Error writing request: {}", err);
    do_finish(err);
  }
}

auto
mc16_client::handle_read_response_header(const bs::error_code &err) -> void {
  // ADS: does this go here?
  [[maybe_unused]] const auto n_cancels_removing =
    deadline.expires_at(steady_timer::time_point::max());
  if (!err) {
    if (const auto resp_hdr_parse{parse(resp_buf, resp_hdr)};
        !resp_hdr_parse.error) {
      lgr.debug("Response header: {}", resp_hdr.summary_serial());
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

auto
mc16_client::do_read_counts() -> void {
  // ADS: this 'resize' seems to belong with the response class
  resp.counts.resize(req.n_intervals);
  asio::async_read(socket, asio::buffer(resp.counts),
                   asio::transfer_exactly(resp.get_counts_n_bytes()),
                   std::bind(&mc16_client::do_finish, this, ph::error));
  [[maybe_unused]] const auto n_cancels =
    deadline.expires_after(chrono::seconds(read_timeout_seconds));
}

auto
mc16_client::do_finish(const std::error_code &err) -> void {
  // same consequence as canceling
  [[maybe_unused]] const auto n_cancels_removing =
    deadline.expires_at(steady_timer::time_point::max());
  lgr.debug("Transaction: {}", err);
  status = err;
  bs::error_code shutdown_ec;  // for non-throwing
  socket.shutdown(tcp::socket::shutdown_both, shutdown_ec);
  bs::error_code socket_close_ec;  // for non-throwing
  socket.close(socket_close_ec);
}

auto
mc16_client::check_deadline() -> void {
  if (!socket.is_open())  // ADS: when can this happen?
    return;

  if (const auto right_now = steady_timer::clock_type::now();
      deadline.expiry() <= right_now) {
    // deadline passed: close socket so remaining async ops are
    // cancelled (see below)
    const auto delta =
      chrono::duration_cast<chrono::seconds>(right_now - deadline.expiry());
    lgr.debug("Error deadline expired by: {}", delta.count());

    bs::error_code shutdown_ec;  // for non-throwing
    socket.shutdown(tcp::socket::shutdown_both, shutdown_ec);
    lgr.debug("Shutdown status: {}", shutdown_ec);
    [[maybe_unused]] const auto n_cancels_removing =
      deadline.expires_at(steady_timer::time_point::max());

    /* ADS: closing here if needed?? */
    bs::error_code socket_close_ec;  // for non-throwing
    socket.close(socket_close_ec);
  }

  // wait again
  deadline.async_wait(std::bind(&mc16_client::check_deadline, this));
}
