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

#include "connection.hpp"

#include "query.hpp"
#include "request_handler.hpp"
#include "response.hpp"

#include <boost/asio.hpp>
#include <boost/system.hpp>

#include <chrono>
#include <compare>  // for operator<=
#include <system_error>

namespace xfrase {

auto
connection::stop() -> void {
  lgr.debug("{} Initiating connection shutdown.", conn_id);
  boost::system::error_code shutdown_ec;  // for non-throwing
  socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, shutdown_ec);
  if (shutdown_ec)
    lgr.warning("{} Shutdown error: {}", conn_id, shutdown_ec);
}

auto
connection::prepare_to_read_query() -> void {
  // ADS: This can't be done in read_query() function as it is
  // recursive. If intervals request, then 'aux_value' is size of
  // query
  qry.resize(req.aux_value);
  // ADS: initialize the counters used while reading the query
  query_remaining = qry.get_n_bytes();
  query_byte = 0;
}

auto
connection::read_request() -> void {
  // as long as lambda is alive, connection instance is too
  auto self(shared_from_this());
  // default capturing 'this' puts names in search
  boost::asio::async_read(
    socket, boost::asio::buffer(req_buf),
    boost::asio::transfer_exactly(request_buffer_size),
    [this, self](const boost::system::error_code ec,
                 [[maybe_unused]] const std::size_t bytes_transferred) {
      // waiting is done; remove deadline for now
      deadline.expires_at(boost::asio::steady_timer::time_point::max());
      if (!ec) {
        if (const auto req_parse_error = parse(req_buf, req);
            !req_parse_error) {
          lgr.debug("{} Received request: {}", conn_id, req.summary());
          handler.handle_request(req, resp_hdr);
          if (!resp_hdr.error()) {
            handler.add_response_size(req, resp_hdr);
            if (req.is_intervals_request()) {
              prepare_to_read_query();
              read_query();
            }
            else {  // req.is_bins_request()
              compute_bins();
            }
          }
          else {
            // response error already assigned in header
            respond_with_error();
          }
        }
        else {
          lgr.warning("{} Request parse error: {}", conn_id, req_parse_error);
          resp_hdr = {req_parse_error, 0};
          respond_with_error();
        }
      }
      else  // problem reading request
        lgr.warning("{} Failed to read request: {}", conn_id, ec);
      // ADS: on error: no new asyncs start; references to this
      // connection disappear; this connection gets destroyed when
      // *both* this handler returns and the timer completes; that
      // destructor destroys the socket
    });
  // ADS: put this before or after the call to asio::async?
  deadline.expires_after(std::chrono::seconds(read_timeout_seconds));
}

auto
connection::read_query() -> void {
  // ADS: I think there's a pattern I'm missing in terms of
  // consistency for setting and clearing the timeouts
  auto self(shared_from_this());
  socket.async_read_some(
    boost::asio::buffer(qry.data() + query_byte, query_remaining),
    [this, self](const boost::system::error_code ec,
                 const std::size_t bytes_transferred) {
      // remove deadline while doing computation
      deadline.expires_at(boost::asio::steady_timer::time_point::max());
      if (!ec) {
        query_remaining -= bytes_transferred;
        query_byte += bytes_transferred;
        if (query_remaining == 0) {
          lgr.debug("{} Finished reading query ({}B)", conn_id, query_byte);
          handler.handle_get_levels(req, qry, resp_hdr, resp);
          lgr.debug("{} Finished computing levels in intervals", conn_id);
          // exiting the read loop -- no deadline for now
          respond_with_header();
        }
        else
          read_query();
      }
      else {
        lgr.warning("{} Error reading query: {}", conn_id, ec);
        resp_hdr = {request_error::error_reading_query, 0};
        // exiting the read loop -- no deadline for now
        respond_with_error();
      }
    });
  deadline.expires_after(std::chrono::seconds(read_timeout_seconds));
}

auto
connection::compute_bins() -> void {
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  handler.handle_get_levels(req, resp_hdr, resp);
  lgr.debug("{} Finished computing levels in bins", conn_id);
  respond_with_header();
}

auto
connection::respond_with_error() -> void {
  const auto compose_error = compose(resp_hdr_buf, resp_hdr);
  if (!compose_error) {
    lgr.warning("{} Responding with error: {}", conn_id, resp_hdr.summary());
    auto self(shared_from_this());
    boost::asio::async_write(
      socket, boost::asio::buffer(resp_hdr_buf),
      [this, self](const boost::system::error_code ec,
                   [[maybe_unused]] const std::size_t bytes_transferred) {
        deadline.expires_at(boost::asio::steady_timer::time_point::max());
        if (ec)
          lgr.error("{} Error responding: {}", conn_id, ec);
        stop();
      });
    deadline.expires_after(std::chrono::seconds(read_timeout_seconds));
  }
  else {
    lgr.error("{} Error responding: {}", conn_id, compose_error);
    stop();
  }
}

auto
connection::respond_with_header() -> void {
  lgr.debug("{} Responding with header: {}", conn_id, resp_hdr.summary());
  const auto compose_error = compose(resp_hdr_buf, resp_hdr);
  if (!compose_error) {
    auto self(shared_from_this());
    boost::asio::async_write(
      socket, boost::asio::buffer(resp_hdr_buf),
      [this, self](const boost::system::error_code ec,
                   [[maybe_unused]] const std::size_t bytes_transferred) {
        deadline.expires_at(boost::asio::steady_timer::time_point::max());
        if (!ec)
          respond_with_levels();
        else {
          lgr.warning("{} Error sending response header: {}", conn_id, ec);
          stop();
        }
      });
    deadline.expires_after(std::chrono::seconds(read_timeout_seconds));
  }
  else {
    lgr.error("{} Error composing response header: {}", conn_id, compose_error);
    stop();
  }
}

auto
connection::respond_with_levels() -> void {
  auto self(shared_from_this());
  boost::asio::async_write(
    socket, boost::asio::buffer(resp.payload),
    [this, self](const boost::system::error_code ec,
                 const std::size_t bytes_transferred) {
      deadline.expires_at(boost::asio::steady_timer::time_point::max());
      if (!ec) {
        lgr.info("{} Responded with levels ({}B)", conn_id, bytes_transferred);
        stop();

        /* ADS: closing here but not sure it makes sense; RAII? See comment in
         * check_deadline */
        boost::system::error_code socket_close_ec;  // for non-throwing
        socket.close(socket_close_ec);
        if (socket_close_ec)
          lgr.warning("{} Socket close error: {}", conn_id, socket_close_ec);
      }
      else
        lgr.warning("{} Error sending levels: {}", conn_id, ec);
    });
  deadline.expires_after(std::chrono::seconds(read_timeout_seconds));
}

auto
connection::check_deadline() -> void {
  if (!socket.is_open())
    return;

  // ADS: use deadline set in place of socket.close so we don't need
  // to check if socket is_open?

  if (deadline.expiry() <= boost::asio::steady_timer::clock_type::now()) {
    // deadline passed: close socket so remaining async ops are
    // cancelled (see comment below)

    stop();

    deadline.expires_at(boost::asio::steady_timer::time_point::max());

    /* ADS: closing here but not sure it makes sense; RAII? see comment in
     * respond_with_levels */
    boost::system::error_code socket_close_ec;  // for non-throwing
    socket.close(socket_close_ec);
    if (socket_close_ec)
      lgr.warning("{} Socket close error: {}", conn_id, socket_close_ec);
  }
  else {
    // ADS: wait again; any issue if the underlying socket is closed?
    // What is the reasoning it will not be closed?
    deadline.async_wait(
      std::bind(&connection::check_deadline, shared_from_this()));
  }
}

}  // namespace xfrase
