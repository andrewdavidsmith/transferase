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

#include "level_container.hpp"
#include "level_element.hpp"
#include "logger.hpp"
#include "query_container.hpp"
#include "request.hpp"
#include "response.hpp"
#include "transfer_stats.hpp"

#include <asio.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <sstream>
#include <utility>  // for std::move

namespace transferase {

struct request_handler;

struct connection : public std::enable_shared_from_this<connection> {
  connection(const connection &) = delete;
  auto
  operator=(const connection &) -> connection & = delete;

  connection(asio::ip::tcp::socket socket_to_move, request_handler &handler,
             logger &lgr, std::uint32_t conn_id) :
    // ADS: 'socket_to_move' has a different name to avoid use after move
    socket{std::move(socket_to_move)}, watchdog_timer{socket.get_executor()},
    handler{handler}, lgr{lgr}, conn_id{conn_id} {
    lgr.info("Connection id: {}. Request endpoint: {}", conn_id,
             (std::ostringstream() << socket.remote_endpoint()).str());
  }

  // clang-format off
  auto watchdog() -> void;      // run the timer
  auto read_request() -> void;  // read request header
  auto read_query() -> void;    // read payload of intervals query
  auto compute_intervals() -> void;
  auto compute_bins() -> void;
  auto compute_windows() -> void;
  auto stop() -> void;                 // shutdown and close
  auto respond_with_header() -> void;  // send success header
  auto respond_with_error() -> void;   // send failure header
  auto respond_with_levels() -> void;  // send payload of levels
  // clang-format on

  auto
  set_deadline(const std::chrono::seconds delta) -> void;

  [[nodiscard]] auto
  get_send_buf() const -> const char *;

  auto
  start() -> void {
    read_request();
    watchdog();
  }

  [[nodiscard]] auto
  is_stopped() const -> bool {
    return !socket.is_open();
  }

  asio::ip::tcp::socket socket;
  asio::steady_timer watchdog_timer;
  std::chrono::steady_clock::time_point deadline{};

  request_handler &handler;               // handles incoming requests
  request req;                            // this connection's request
  request_buffer req_buf{};               // bffer for the request
  response_header resp_hdr;               // header of the response
  response_header_buffer resp_hdr_buf{};  // buffer for the resp_hdr
  query_container query;                  // 'query' is its own buffer

  // ADS: keeping both resp and resp_cov is intended (so far)
  level_container<level_element_t> resp;
  level_container<level_element_covered_t> resp_cov;

  logger &lgr;
  std::uint32_t conn_id{};  // identifies this connection for logging purposes

  /// ADS: 'work_timeout_sec' is the timeout while doing work. This is a high
  /// value that essentially removes the timeout. The 'work' is blocking, so
  /// there should be no opportunity for the timer to wake up. In the future,
  /// file reads might be async, but for now as long as the work is going, we
  /// want it to keep going. If the work encounters a problem, it will move to
  /// respond with an error, and return to async ops.
  std::chrono::seconds work_timeout_sec{300};

  /// ADS: 'comm_timeout_sec' is the wait time between consecutive
  /// 'async_read_some' and 'async_write_some' calls within the 'async_read'
  /// and 'async_write'. If these see no activity for 'comm_timeout_sec'
  /// seconds, then a problems is assumed.
  std::chrono::seconds comm_timeout_sec{10};
  bool timeout_happened{false};

  /// ADS: Keeping stats on both the query and the reply
  transfer_stats query_stats{};
  transfer_stats reply_stats{};
};

}  // namespace transferase

#endif  // LIB_CONNECTION_HPP_
