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

#include "request.hpp"
#include "request_handler.hpp"
#include "response.hpp"

#include <boost/asio.hpp>

auto
connection::prepare_to_read_offsets() -> void {
  // This function is needed because this can't be done in the
  // read_offsets() function as it is recursive
  req.offsets.resize(req.n_intervals);           // get space for offsets
  offset_remaining = req.get_offsets_n_bytes();  // init counters
  offset_byte = 0;                               // should be init to this
}

auto
connection::read_request() -> void {
  // as long as lambda is alive, connection instance is too
  auto self(shared_from_this());
  // default capturing 'this' puts names in search
  boost::asio::async_read(
    socket, boost::asio::buffer(req_hdr_buf),
    boost::asio::transfer_exactly(request_header_buf_size),
    [this, self](const boost::system::error_code ec,
                 [[maybe_unused]] const std::size_t bytes_transferred) {
      // waiting is done; remove deadline for now
      deadline.expires_at(boost::asio::steady_timer::time_point::max());
      if (!ec) {
        if (const auto req_hdr_parse{parse(req_hdr_buf, req_hdr)};
            !req_hdr_parse.error) {
          lgr.debug("{} Received request header: {}", connection_id,
                    req_hdr.summary());
          handler.handle_header(req_hdr, resp_hdr);
          if (!resp_hdr.error()) {
            if (req_hdr.is_intervals_request()) {
              if (const auto req_parse =
                    from_chars(req_hdr_parse.ptr, cend(req_hdr_buf), req);
                  !req_parse.error) {
                prepare_to_read_offsets();
                read_offsets();
              }
              else {
                lgr.warning("{} Request parse error: {}", connection_id,
                            req_parse.error);
                resp_hdr = {req_parse.error, 0};
                respond_with_error();
              }
            }
            else {  // only possibility is req_hdr.is_bins_request()
              if (const auto req_parse =
                    from_chars(req_hdr_parse.ptr, cend(req_hdr_buf), bins_req);
                  !req_parse.error) {
                handler.add_response_size_for_bins(req_hdr, bins_req, resp_hdr);
                compute_bins();
              }
              else {
                lgr.warning("{} Bins request parse error: {}", connection_id,
                            req_parse.error);
                resp_hdr = {req_parse.error, 0};
                respond_with_error();
              }
            }
          }
          else {
            lgr.warning("{} Responding with error: {}", connection_id,
                        resp_hdr.summary());
            respond_with_error();  // response error already assigned
          }
        }
        else {
          lgr.warning("{} Request parse error: {}", connection_id,
                      req_hdr_parse.error);
          resp_hdr = {req_hdr_parse.error, 0};
          respond_with_error();
        }
      }
      else  // problem reading request
        lgr.warning("{} Failed to read request: {}", connection_id, ec);
      // ADS: on error: no new asyncs start; references to this
      // connection disappear; this connection gets destroyed when
      // this handler returns; that destructor destroys the socket
    });
  // ADS: put this before or after the call to asio::async?
  deadline.expires_after(std::chrono::seconds(read_timeout_seconds));
}

auto
connection::read_offsets() -> void {
  auto self(shared_from_this());
  socket.async_read_some(
    boost::asio::buffer(req.get_offsets_data() + offset_byte, offset_remaining),
    [this, self](const boost::system::error_code ec,
                 const std::size_t bytes_transferred) {
      // remove deadline while doing computation
      deadline.expires_at(boost::asio::steady_timer::time_point::max());
      if (!ec) {
        offset_remaining -= bytes_transferred;
        offset_byte += bytes_transferred;
        if (offset_remaining == 0) {
          lgr.debug("{} Finished reading offsets ({} Bytes)", connection_id,
                    offset_byte);
          handler.handle_get_counts(req_hdr, req, resp_hdr, resp);
          lgr.debug(
            "{} Finished methylation counts. Responding with header: {}",
            connection_id, resp_hdr.summary());
          // exiting the read loop -- no deadline for now
          respond_with_header();
        }
        else
          read_offsets();
      }
      else {
        lgr.warning("{} Error reading offsets: {}", connection_id, ec);
        resp_hdr = {request_error::lookup_error_reading_offsets, 0};
        // exiting the read loop -- no deadline for now
        respond_with_error();
      }
    });
  deadline.expires_after(std::chrono::seconds(read_timeout_seconds));
}

auto
connection::compute_bins() -> void {
  deadline.expires_at(boost::asio::steady_timer::time_point::max());
  handler.handle_get_bins(req_hdr, bins_req, resp_hdr, resp);
  lgr.debug("{} Finished methylation bin counts. Responding with header: {}",
            connection_id, resp_hdr.summary());
  respond_with_header();
}

auto
connection::respond_with_error() -> void {
  if (const auto resp_hdr_compose{compose(resp_hdr_buf, resp_hdr)};
      !resp_hdr_compose.error) {
    auto self(shared_from_this());
    boost::asio::async_write(
      socket, boost::asio::buffer(resp_hdr_buf),
      [this, self](const boost::system::error_code ec,
                   [[maybe_unused]] const std::size_t bytes_transferred) {
        deadline.expires_at(boost::asio::steady_timer::time_point::max());
        if (!ec) {
          lgr.warning(
            "{} Responded with error: {}. Initiating connection shutdown.",
            connection_id, resp_hdr.summary());
        }
        else {
          lgr.error("{} Error responding: {}. Initiating connection shutdown.",
                    connection_id, ec);
        }
        boost::system::error_code shutdown_ec;  // for non-throwing
        socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both,
                        shutdown_ec);
        if (shutdown_ec)
          lgr.warning("{} Shutdown error: {}", connection_id, shutdown_ec);
      });
    deadline.expires_after(std::chrono::seconds(read_timeout_seconds));
  }
  else {
    lgr.error("{} Error responding: {}. Initiating connection shutdown.",
              connection_id, resp_hdr_compose.error);
    boost::system::error_code shutdown_ec;  // for non-throwing
    socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, shutdown_ec);
    if (shutdown_ec)
      lgr.warning("{} Shutdown error: {}", connection_id, shutdown_ec);
  }
}

auto
connection::respond_with_header() -> void {
  if (const auto resp_hdr_compose{compose(resp_hdr_buf, resp_hdr)};
      !resp_hdr_compose.error) {
    auto self(shared_from_this());
    boost::asio::async_write(
      socket, boost::asio::buffer(resp_hdr_buf),
      [this, self](const boost::system::error_code ec,
                   [[maybe_unused]] const std::size_t bytes_transferred) {
        deadline.expires_at(boost::asio::steady_timer::time_point::max());
        if (!ec)
          respond_with_counts();
        else {
          lgr.warning("{} Error sending response header: {}. "
                      "Initiating connection shutdown.",
                      connection_id, ec);
          boost::system::error_code shutdown_ec;  // for non-throwing
          socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both,
                          shutdown_ec);
          if (shutdown_ec)
            lgr.warning("{} Shutdown error: {}", connection_id, shutdown_ec);
        }
      });
    deadline.expires_after(std::chrono::seconds(read_timeout_seconds));
  }
  else {
    lgr.error(
      "{} Error composing response header: {}. Initiating connection shutdown.",
      connection_id, resp_hdr_compose.error);
    boost::system::error_code shutdown_ec;  // for non-throwing
    socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, shutdown_ec);
    if (shutdown_ec)
      lgr.warning("{} Shutdown error: {}", connection_id, shutdown_ec);
  }
}

auto
connection::respond_with_counts() -> void {
  auto self(shared_from_this());
  boost::asio::async_write(
    socket, boost::asio::buffer(resp.payload),
    [this, self](const boost::system::error_code ec,
                 const std::size_t bytes_transferred) {
      deadline.expires_at(boost::asio::steady_timer::time_point::max());
      if (!ec) {
        lgr.info("{} Responded with counts ({} Bytes). Initiating "
                 "connection shutdown.",
                 connection_id, bytes_transferred);
        boost::system::error_code shutdown_ec;  // for non-throwing
        socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both,
                        shutdown_ec);
        if (shutdown_ec)
          lgr.warning("{} Shutdown error: {}", connection_id, shutdown_ec);
        boost::system::error_code socket_close_ec;  // for non-throwing
        socket.close(socket_close_ec);
        if (socket_close_ec)
          lgr.warning("{} Socket close error: {}", connection_id,
                      socket_close_ec);
      }
      else
        lgr.warning("{} Error sending counts: {}", connection_id, ec);
    });
  deadline.expires_after(std::chrono::seconds(read_timeout_seconds));
}

auto
connection::check_deadline() -> void {
  if (!socket.is_open())
    return;

  if (deadline.expiry() <= boost::asio::steady_timer::clock_type::now()) {
    // deadline passed: close socket so remaining async ops are
    // cancelled (see comment below)

    boost::system::error_code shutdown_ec;  // for non-throwing
    socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, shutdown_ec);
    if (shutdown_ec)
      lgr.warning("{} Shutdown error: {}", connection_id, shutdown_ec);
    deadline.expires_at(boost::asio::steady_timer::time_point::max());

    /* ADS: closing here makes sense */
    boost::system::error_code socket_close_ec;  // for non-throwing
    socket.close(socket_close_ec);
    if (socket_close_ec)
      lgr.warning("{} Socket close error: {}", connection_id, socket_close_ec);
  }

  // ADS: wait again; any issue if the underlying socket is closed?
  deadline.async_wait(std::bind(&connection::check_deadline, this));
}
