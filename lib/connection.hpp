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

#ifndef LIB_CONNECTION_HPP_
#define LIB_CONNECTION_HPP_

#include "level_container_md.hpp"
#include "level_element.hpp"
#include "logger.hpp"
#include "query_container.hpp"
#include "request.hpp"
#include "response.hpp"

#include <asio.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>  // for std::bind
#include <limits>      // for std::numeric_limits
#include <memory>
#include <string>
#include <utility>  // for std::move

namespace transferase {

struct request_handler;

struct connection : public std::enable_shared_from_this<connection> {
  connection(const connection &) = delete;
  connection &
  operator=(const connection &) = delete;

  connection(asio::ip::tcp::socket socket_to_move, request_handler &handler,
             logger &lgr, std::uint32_t conn_id) :
    // 'socket' below can get confused if arg has exact same name
    socket{std::move(socket_to_move)}, deadline{socket.get_executor()},
    handler{handler}, lgr{lgr}, conn_id{conn_id} {
    lgr.info("Connection id: {}. Request endpoint: {}", conn_id,
             (std::ostringstream() << socket.remote_endpoint()).str());
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

  [[nodiscard]] auto
  get_outgoing_n_bytes() const -> std::uint32_t {
    return req.is_covered_request() ? resp_cov.get_n_bytes()
                                    : resp.get_n_bytes();
  }

  auto
  prepare_to_write_response_payload() -> void {
    outgoing_bytes_remaining = get_outgoing_n_bytes();  // init counters
    outgoing_bytes_sent = 0;
  }

  [[nodiscard]] auto
  get_outgoing_data_buffer() const noexcept -> const char * {
    return req.is_covered_request()
             ? reinterpret_cast<const char *>(resp_cov.data())
             : reinterpret_cast<const char *>(resp.data());
  }

  asio::ip::tcp::socket socket;  // this connection's socket
  asio::steady_timer deadline;
  request_handler &handler;  // handles incoming requests
  request_buffer req_buf{};
  request req;  // this connection's request
  response_header_buffer resp_hdr_buf{};
  response_header resp_hdr;  // header of the response

  // ADS: keeping instance of both below bc alternatives aren't that much
  // better -- reminder that this was a choice and not an accident.
  level_container_md<level_element_t> resp;
  level_container_md<level_element_covered_t> resp_cov;

  logger &lgr;
  std::uint32_t conn_id{};  // identifer for this connection
  std::uint32_t read_timeout_seconds{10};

  // These help keep track of where we are in the incoming offsets;
  // they might best be associated with the request.
  transferase::query_container query;
  std::size_t query_byte{};
  std::size_t query_remaining{};

  std::size_t outgoing_bytes_sent{};
  std::size_t outgoing_bytes_remaining{};

  std::size_t n_writes{};
  std::size_t min_write_size{std::numeric_limits<std::size_t>::max()};
  std::size_t max_write_size{};

  std::size_t n_reads{};
  std::size_t min_read_size{std::numeric_limits<std::size_t>::max()};
  std::size_t max_read_size{};
};

}  // namespace transferase

#endif  // LIB_CONNECTION_HPP_
