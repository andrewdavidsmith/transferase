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

#ifndef SRC_CONNECTION_HPP_
#define SRC_CONNECTION_HPP_

#include "logger.hpp"
#include "query_container.hpp"
#include "request.hpp"
#include "response.hpp"

#include <boost/asio.hpp>          // for tcp, steady_timer
#include <boost/lexical_cast.hpp>  // for lexical_cast

#include <cstddef>
#include <cstdint>
#include <functional>  // for std::bind
#include <memory>
#include <string>
#include <utility>  // for std::move

namespace transferase {

struct request_handler;

struct connection : public std::enable_shared_from_this<connection> {
  connection(const connection &) = delete;
  connection &
  operator=(const connection &) = delete;

  explicit connection(boost::asio::ip::tcp::socket socket_,
                      request_handler &handler, logger &lgr,
                      std::uint32_t conn_id) :
    // socket used below gets confused if arg has exact same name
    socket{std::move(socket_)}, deadline{socket.get_executor()},
    handler{handler}, lgr{lgr}, conn_id{conn_id} {
    lgr.info("Connection id: {}. Request endpoint: {}", conn_id,
             boost::lexical_cast<std::string>(socket.remote_endpoint()));
  }

  auto
  start() -> void {
    read_request();  // start first async op; sets deadline

    // This might best be done at the end of read_request, then
    // cleared and re-done at the appropriate times in read_offsets
    // NOTE: deadline already set for this one inside read_request().
    // Completion handler must be bound to a shared pointer so the
    // session remains alive for the timer
    deadline.async_wait(
      std::bind(&connection::check_deadline, shared_from_this()));
  }

  auto
  stop() -> void;  // shutdown the socket

  // Allocate space for offsets and initialize the variables that
  // track where we are in the buffer as data arrives.
  auto
  prepare_to_read_query() -> void;

  auto
  read_request() -> void;  // read 'request'
  auto
  read_query() -> void;  // read the 'query' part of request
  auto
  compute_bins() -> void;  // do the computation for bins

  auto
  respond_with_header() -> void;  // send success header
  auto
  respond_with_error() -> void;  // send error header
  auto
  respond_with_levels() -> void;  // send levels

  auto
  check_deadline() -> void;

  boost::asio::ip::tcp::socket socket;  // this connection's socket
  boost::asio::steady_timer deadline;
  request_handler &handler;  // handles incoming requests
  request_buffer req_buf{};
  request req;  // this connection's request
  response_header_buffer resp_hdr_buf{};
  response_header resp_hdr;  // header of the response
  response_payload resp;     // response to send back
  logger &lgr;
  std::uint32_t conn_id{};  // identifer for this connection
  std::uint32_t read_timeout_seconds{10};

  // These help keep track of where we are in the incoming offsets;
  // they might best be associated with the request.
  transferase::query_container query;
  std::size_t query_byte{};
  std::size_t query_remaining{};
};

}  // namespace transferase

#endif  // SRC_CONNECTION_HPP_
