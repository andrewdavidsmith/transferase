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

#include <cstdint>  // std::uint32_t, etc.
#include <print>
#include <string>

using std::println;
using std::size_t;
using std::string;
using std::uint32_t;

namespace asio = boost::asio;
namespace bs = boost::system;

using tcp = asio::ip::tcp;

typedef mc16_log_level lvl;

auto
connection::prepare_to_read_offsets() -> void {
  req.offsets.resize(req.n_intervals);  // get space for offsets
  offset_remaining = req.get_offsets_n_bytes();  // init counters
  offset_byte = 0;  // shouldn't need this
}

auto
connection::read_request() -> void {
  // as long as lambda is alive, connection instance is too
  auto self(shared_from_this());
  // default capturing 'this' puts names in search
  asio::async_read(
    socket, asio::buffer(req_buf), asio::transfer_exactly(request_buf_size),
    [this, self](const bs::error_code ec,
                 [[maybe_unused]] const size_t bytes_transferred) {
      if (!ec) {
        if (const auto req_hdr_parse{parse(req_buf, req_hdr)};
            !req_hdr_parse.error) {
          fl.log<lvl::debug>("Received request header: {}",
                             req_hdr.summary_serial());
          handler.handle_header(req_hdr, resp_hdr);
          if (!resp_hdr.error()) {
            if (const auto req_body_parse =
                  from_chars(req_hdr_parse.ptr, cend(req_buf), req);
                !req_body_parse.error) {
              prepare_to_read_offsets();
              read_offsets();
            }
            else {
              fl.log<lvl::warning>("Request body parse error: {}",
                                   req_body_parse.error);
              resp_hdr = {req_body_parse.error, 0};
              respond_with_error();
            }
          }
          else {
            fl.log<lvl::warning>("Responding with error: {}",
                                 resp_hdr.summary_serial());
            respond_with_error();  // response error already assigned
          }
        }
        else {
          fl.log<lvl::warning>("Request parse error: {}", req_hdr_parse.error);
          resp_hdr = {req_hdr_parse.error, 0};
          respond_with_error();
        }
      }
      // ADS: on error: no new asyncs start; references to this
      // connection disappear; this connection gets destroyed when
      // this handler returns; that destructor destroys the socket
    });
}

auto
connection::read_offsets() -> void {
  auto self(shared_from_this());
  socket.async_read_some(
    asio::buffer(req.get_offsets_data() + offset_byte, offset_remaining),
    [this, self](const bs::error_code ec, const size_t bytes_transferred) {
      if (!ec) {
        offset_remaining -= bytes_transferred;
        offset_byte += bytes_transferred;
        if (offset_remaining == 0) {
          fl.log<lvl::debug>("Finished reading offsets [{} Bytes].",
                             offset_byte);
          handler.handle_get_counts(req_hdr, req, resp_hdr, resp);
          fl.log<lvl::debug>("Finished methylation counts. "
                             "Responding with header: {}",
                             resp_hdr.summary_serial());
          respond_with_header();
        }
        else
          read_offsets();
      }
      else {
        fl.log<lvl::warning>("Error reading offsets: {}", ec);
        resp_hdr = {request_error::lookup_error_reading_offsets, 0};
        respond_with_error();
      }
    });
}

auto
connection::respond_with_error() -> void {
  auto self(shared_from_this());
  if (const auto resp_hdr_compose{compose(resp_buf, resp_hdr)};
      !resp_hdr_compose.error) {
    asio::async_write(
      socket, asio::buffer(resp_buf),
      [this, self](const bs::error_code ec,
                   [[maybe_unused]] const size_t bytes_transferred) {
        if (!ec) {
          fl.log<lvl::warning>("Responded with error: {}. "
                               "Initiating connection shutdown.",
                               resp_hdr.summary_serial());
          socket.shutdown(tcp::socket::shutdown_both);
        }
      });
  }
  else {
    fl.log<lvl::error>("Error responding: {}. "
                       "Initiating connection shutdown.",
                       resp_hdr_compose.error);
    socket.shutdown(tcp::socket::shutdown_both);
  }
}

auto
connection::respond_with_header() -> void {
  auto self(shared_from_this());
  if (const auto resp_hdr_compose{compose(resp_buf, resp_hdr)};
      !resp_hdr_compose.error) {
    asio::async_write(
      socket, asio::buffer(resp_buf),
      [this, self](const bs::error_code ec,
                   [[maybe_unused]] const size_t bytes_transferred) {
        if (!ec)
          respond_with_counts();
        else {
          fl.log<lvl::warning>("Error sending response header: {}."
                               "Initiating connection shutdown.",
                               ec);
          socket.shutdown(tcp::socket::shutdown_both);
        }
      });
  }
  else {
    fl.log<lvl::error>("Error composing response header: {}."
                       "Initiating connection shutdown.",
                       resp_hdr_compose.error);
    socket.shutdown(tcp::socket::shutdown_both);
  }
}

auto
connection::respond_with_counts() -> void {
  auto self(shared_from_this());
  asio::async_write(
    socket, asio::buffer(resp.counts),
    [this, self](const bs::error_code ec, const size_t bytes_transferred) {
      if (!ec) {
        fl.log<lvl::info>("Responded with counts [{} Bytes]. "
                          "Initiating connection shutdown.",
                          bytes_transferred);
        socket.shutdown(tcp::socket::shutdown_both);
      }
    });
}
