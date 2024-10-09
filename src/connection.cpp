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
#include "status_code.hpp"

#include <cstdint>  // std::uint32_t, etc.
#include <print>
#include <string>
#include <utility>  // memcpy

using std::println;
using std::size_t;
using std::string;
using std::uint32_t;

namespace asio = boost::asio;
namespace bs = boost::system;

using tcp = asio::ip::tcp;

auto
connection::prepare_to_read_offsets() -> void {
  req.offsets.resize(req.n_intervals);  // get space for offsets
  offset_remaining = req.get_offsets_n_bytes();  // init counters
  offset_byte = 0;
}

auto
connection::read_request() -> void {
  // as long as lambda is alive, connection instance is too
  auto self(shared_from_this());
  // default capturing 'this' puts names in search
  asio::async_read(
    socket, asio::buffer(req.buf), asio::transfer_exactly(req.buf_size),
    [this, self](const bs::error_code ec,
                 [[maybe_unused]] const size_t bytes_transferred) {
      if (!ec) {
        const auto result = req.from_buffer();
        if (result == status_code::ok) {
          if (verbose)
            println("Received request: {}", req.summary_serial());
          handler.handle_header(req, resp);
          if (resp.is_good()) {
            prepare_to_read_offsets();
            read_offsets();
          }
          else {
            if (verbose)
              println("Responding with error: {}", resp.summary_serial());
            respond_with_error();
          }
        }
        else {  // if (result != status_code::ok) {
          if (verbose)
            println("Request parse error: {}", result);
          resp = response::bad_request();
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
          println("Finished reading offsets [{} Bytes].", offset_byte);
          handler.handle_get_counts(req, resp);
          if (verbose)
            println("Finished methylation counts.");
          if (verbose)
            println("Responding with header: {}", resp.summary_serial());
          respond_with_header();
        }
        // ADS: keep conditions exclusive
        else
          read_offsets();
      }
      else {
        println("Error reading offsets: {}", ec.message());
        respond_with_error();
      }
    });
}

auto
connection::respond_with_header() -> void {
  auto self(shared_from_this());
  resp.to_buffer();  // ADS: must do this before write
  asio::async_write(
    socket, asio::buffer(resp.buf),
    [this, self](const bs::error_code ec,
                 [[maybe_unused]] const size_t bytes_transferred) {
      if (!ec)
        respond_with_counts();
      else {
        if (verbose)
          println("Error responding with header: {}"
                  "Initiating connection shutdown.",
                  ec.message());
        bs::error_code ignored_ec{};  // any reason to handle this?
        // graceful connection closure
        socket.shutdown(tcp::socket::shutdown_both, ignored_ec);
      }
    });
}

auto
connection::respond_with_counts() -> void {
  auto self(shared_from_this());
  asio::async_write(
    socket, asio::buffer(resp.counts),
    [this, self](const bs::error_code ec, const size_t bytes_transferred) {
      if (!ec) {
        if (verbose)
          println("Responded with counts [{} Bytes]. "
                  "Initiating connection shutdown.",
                  bytes_transferred);
        bs::error_code ignored_ec{};  // any reason to handle this?
        // graceful connection closure
        socket.shutdown(tcp::socket::shutdown_both, ignored_ec);
      }
    });
}

auto
connection::respond_with_error() -> void {
  auto self(shared_from_this());
  resp.to_buffer();  // ADS: must do this before write
  asio::async_write(
    socket, asio::buffer(resp.buf),
    [this, self](const bs::error_code ec, const size_t bytes_transferred) {
      if (!ec) {
        if (verbose)
          println("Responded with error [{} Bytes]. "
                  "Initiating connection shutdown.",
                  bytes_transferred);
        bs::error_code ignored_ec{};  // any reason to handle this?
        // graceful connection closure
        socket.shutdown(tcp::socket::shutdown_both, ignored_ec);
      }
    });
}
