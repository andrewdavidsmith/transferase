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

#include "logging.hpp"
#include "request.hpp"
#include "response.hpp"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>  // std::shared_ptr
#include <utility>  // std::move

struct request_handler;

struct connection : public std::enable_shared_from_this<connection> {
  connection(const connection &) = delete;
  connection &operator=(const connection &) = delete;

  explicit connection(boost::asio::ip::tcp::socket socket_,
                      request_handler &handler, file_logger &fl,
                      std::uint32_t connection_id) :
    // socket used below gets confused if arg has exact same name
    socket{std::move(socket_)}, handler{handler}, fl{fl},
    connection_id{connection_id} {
    fl.log<mc16_log_level::info>(
      "Connection id: {}. Request endpoint: {}", connection_id,
      boost::lexical_cast<std::string>(socket.remote_endpoint()));
  }

  auto start() -> void { read_request(); }  // start first async op

  auto prepare_to_read_offsets() -> void;

  auto read_request() -> void;  // read request
  auto read_offsets() -> void;  // read offsets part of request

  auto respond_with_header() -> void;  // write good header
  auto respond_with_error() -> void;  // write error header
  auto respond_with_counts() -> void;  // write counts

  boost::asio::ip::tcp::socket socket;  // this connection's socket
  request_handler &handler;  // handles incoming requests
  request_buffer req_buf;
  request_header req_hdr;  // this connection's request header
  request req;  // this connection's request
  response_buffer resp_buf;
  response_header resp_hdr;  // header of the response
  response resp;  // response to send back
  file_logger &fl;
  std::uint32_t connection_id{};

  // These help keep track of where we are in the incoming offsets;
  // they might best be associated with the request.
  std::size_t offset_byte{};
  std::size_t offset_remaining{};
};

typedef std::shared_ptr<connection> connection_ptr;

#endif  // SRC_CONNECTION_HPP_
