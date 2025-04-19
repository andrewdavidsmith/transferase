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

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>  // for std::bind
#include <limits>      // for std::numeric_limits
#include <memory>
#include <new>
#include <sstream>
#include <utility>  // for std::move

namespace transferase {

struct request_handler;

struct comm_stats {
  std::uint32_t n_comms{};
  std::uint32_t comm_bytes{};
  std::uint32_t min_comm_size{std::numeric_limits<std::uint32_t>::max()};
  std::uint32_t max_comm_size{};

  auto
  update(const std::uint32_t n_bytes) noexcept -> void {
    ++n_comms;
    comm_bytes += n_bytes;
    max_comm_size = std::max(max_comm_size, n_bytes);
    min_comm_size = std::min(min_comm_size, n_bytes);
  }

  [[nodiscard]] auto
  str() const noexcept -> std::string {
    static constexpr auto fmt = "{}B, reads={}, max={}B, min={}B, mean={}B";
    return std::format(fmt, comm_bytes, n_comms, max_comm_size, min_comm_size,
                       comm_bytes / n_comms);
  }
};

struct connection : public std::enable_shared_from_this<connection> {
  connection(const connection &) = delete;
  connection &
  operator=(const connection &) = delete;

  connection(asio::ip::tcp::socket socket_to_move, request_handler &handler,
             logger &lgr, std::uint32_t conn_id) :
    // 'socket' below can get confused if arg has exact same name
    socket{std::move(socket_to_move)}, watchdog_timer{socket.get_executor()},
    handler{handler}, lgr{lgr}, conn_id{conn_id} {
    lgr.info("Connection id: {}. Request endpoint: {}", conn_id,
             (std::ostringstream() << socket.remote_endpoint()).str());
  }

  auto
  start() -> void {
    read_request();  // start first async op; sets deadline
    watchdog();
  }

  auto
  watchdog() -> void;

  auto
  stop() -> void;  // shutdown the socket

  auto
  is_stopped() const -> bool {
    return !socket.is_open();
  }

  // Allocate space for offsets, initialize tracking variables, the start
  // rading query
  auto
  init_read_query() -> void;

  auto
  read_request() -> void;  // read 'request'
  auto
  read_query() -> void;  // read the 'query' part of request

  // Compute the levels for intervals and bins
  // clang-format off
  auto compute_intervals() noexcept -> void;
  auto compute_bins() noexcept -> void;
  // clang-format on

  auto
  respond_with_header() -> void;  // send success header
  auto
  respond_with_error() -> void;  // send error header
  auto
  respond_with_levels() -> void;  // send levels

  auto
  check_deadline() -> void;

  [[nodiscard]] auto
  get_response_size() const -> std::uint32_t {
    return req.is_covered_request() ? resp_cov.get_n_bytes()
                                    : resp.get_n_bytes();
  }

  auto
  init_write_response() -> void;

  [[nodiscard]] auto
  get_send_buf(const std::uint32_t bytes_sent) const noexcept -> const char *;

  auto
  set_deadline(const std::chrono::seconds delta) -> void;

  asio::ip::tcp::socket socket;  // this connection's socket
  asio::steady_timer watchdog_timer;
  // update the deadline, rather than the expiry of the watchdog_timer
  std::chrono::steady_clock::time_point deadline{};

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

  std::chrono::seconds work_timeout_sec{120};
  std::chrono::seconds comm_timeout_sec{10};

  // These help keep track of where we are in the incoming offsets;
  // they might best be associated with the request.
  transferase::query_container query;
  std::size_t query_byte{};
  std::size_t query_remaining{};

  std::size_t levels_byte{};
  std::size_t levels_remaining{};

  comm_stats reply_stats{};
  comm_stats query_stats{};
};

}  // namespace transferase

#endif  // LIB_CONNECTION_HPP_
