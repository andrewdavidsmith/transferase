/* MIT License
 *
 * Copyright (c) 2024-2025 Andrew D Smith
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

#include "customize_asio.hpp"
#include "query_container.hpp"
#include "request_handler.hpp"
#include "response.hpp"
#include "transfer_stats.hpp"

#include <asio.hpp>

#include <algorithm>
#include <chrono>
#include <compare>  // for operator<=
#include <string>
#include <system_error>

namespace transferase {

auto
connection::set_deadline(const std::chrono::seconds delta) -> void {
  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(delta);
}

[[nodiscard]] auto
connection::get_send_buf() const -> const char * {
  // NOLINTBEGIN (*-reinterpret-cast)
  return req.is_covered_request()
           ? reinterpret_cast<const char *>(resp_cov.data())
           : reinterpret_cast<const char *>(resp.data());
  // NOLINTEND (*-reinterpret-cast)
}

auto
connection::compute_intervals() -> void {
  if (req.is_covered_request())
    handler.intervals_get_levels(req, query, resp_hdr, resp_cov);
  else
    handler.intervals_get_levels(req, query, resp_hdr, resp);

  if (resp_hdr.status) {
    lgr.warning("{} Error computing levels: {}", conn_id,
                resp_hdr.status.message());
    respond_with_error();
    return;
  }

  lgr.debug("{} Finished computing levels in intervals", conn_id);
  respond_with_header();
}

auto
connection::compute_bins() -> void {
  if (req.is_covered_request())
    handler.bins_get_levels(req, resp_hdr, resp_cov);
  else
    handler.bins_get_levels(req, resp_hdr, resp);

  if (resp_hdr.status) {
    lgr.warning("{} Error computing levels: {}", conn_id,
                resp_hdr.status.message());
    respond_with_error();
    return;
  }

  lgr.debug("{} Finished computing levels in bins", conn_id);
  respond_with_header();
}

auto
connection::read_request() -> void {
  set_deadline(comm_timeout_sec);
  auto self = shared_from_this();
  asio::async_read(
    socket, asio::buffer(req_buf), asio::transfer_exactly(request_buffer_size),
    [this, self](const auto ec, auto) {
      if (ec) {
        lgr.warning("{} Failed to read request: {}", conn_id, ec);
        stop();
        return;
      }

      if (const auto parse_err = parse(req_buf, req); parse_err) {
        lgr.warning("{} Request parse error: {}", conn_id, parse_err);
        resp_hdr = {parse_err, 0};
        respond_with_error();
        return;
      }

      set_deadline(work_timeout_sec);  // handle_request might need time
      lgr.debug("{} Received request: {}", conn_id, req.summary());
      handler.handle_request(req, resp_hdr);
      if (resp_hdr.error()) {
        respond_with_error();
        return;
      }

      if (req.is_intervals_request())
        read_query();
      else  // only alternative is bins request
        compute_bins();
    });
}

auto
connection::read_query() -> void {
  query.resize(req.aux_value);
  set_deadline(comm_timeout_sec);
  auto self = shared_from_this();
  asio::async_read(
    socket, asio::buffer(query.data(), query.n_bytes()),
    [this, self](const auto ec, const auto n_bytes) -> std::size_t {
      query_stats.update(n_bytes);
      set_deadline(comm_timeout_sec);
      auto completion_condition = transfer_all_higher_max();
      return is_stopped() ? 0 : completion_condition(ec, n_bytes);
    },
    [this, self](const auto ec, const auto n_bytes) {
      query_stats.update(n_bytes);
      if (ec) {
        lgr.warning("{} Error reading query: {}", conn_id, ec);
        resp_hdr = {request_error_code::error_reading_query, 0};
        respond_with_error();
        return;
      }
      set_deadline(work_timeout_sec);
      lgr.debug("{} Finished reading query ({})", conn_id, query_stats.str());
      compute_intervals();
    });
}

auto
connection::respond_with_error() -> void {
  lgr.warning("{} Responding with error: {}", conn_id, resp_hdr.summary());
  if (const auto comp_err = compose(resp_hdr_buf, resp_hdr); comp_err) {
    lgr.error("{} Error responding: {}", conn_id, comp_err);
    stop();
    return;
  }

  set_deadline(comm_timeout_sec);
  auto self = shared_from_this();
  // clang-format off
  asio::async_write(socket, asio::buffer(resp_hdr_buf),
    [this, self](const auto ec, auto) {
      if (ec)
        lgr.error("{} Error responding: {}", conn_id, ec);
      stop();
    });
  // clang-format on
}

auto
connection::respond_with_header() -> void {
  lgr.debug("{} Responding with header: {}", conn_id, resp_hdr.summary());
  if (const auto ec = compose(resp_hdr_buf, resp_hdr); ec) {
    lgr.error("{} Error composing response header: {}", conn_id, ec);
    stop();
    return;
  }

  set_deadline(comm_timeout_sec);
  auto self = shared_from_this();
  // clang-format off
  asio::async_write(socket, asio::buffer(resp_hdr_buf),
    [this, self](const auto ec, auto) {
      if (ec) {
        lgr.warning("{} Error sending header: {}", conn_id, ec);
        stop();
        return;
      }
      respond_with_levels();
    });
  // clang-format on
}

auto
connection::respond_with_levels() -> void {
  set_deadline(comm_timeout_sec);
  auto self = shared_from_this();
  asio::async_write(
    socket, asio::buffer(get_send_buf(), get_response_size()),
    // completion condition
    [this, self](const auto ec, const auto n_bytes) -> std::size_t {
      reply_stats.update(n_bytes);
      set_deadline(comm_timeout_sec);
      auto completion_condition = transfer_all_higher_max();
      return is_stopped() ? 0 : completion_condition(ec, n_bytes);
    },
    // completion token
    [this, self](const auto ec, auto) {
      reply_stats.update(n_bytes);
      if (ec)
        lgr.warning("{} Error sending levels: {}", conn_id, ec);
      else
        lgr.info("{} Response complete ({})", conn_id, reply_stats.str());
      stop();
    });
}

auto
connection::watchdog() -> void {
  auto self = shared_from_this();
  watchdog_timer.expires_at(deadline);
  watchdog_timer.async_wait([self](auto) {
    if (!self->is_stopped()) {
      if (self->deadline < std::chrono::steady_clock::now()) {
        self->timeout_happened = true;
        self->stop();
        return;
      }
      self->watchdog();
    }
  });
}

auto
connection::stop() -> void {
  if (timeout_happened)
    lgr.warning("{} Timeout happened", conn_id);

  lgr.debug("{} Initiating connection shutdown", conn_id);
  std::error_code ec;
  (void)socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
  if (ec)
    lgr.warning("{} Shutdown error: {}", conn_id, ec);

  (void)socket.close(ec);
  if (ec)
    lgr.warning("{} Socket close error: {}", conn_id, ec);

  watchdog_timer.cancel();
}

}  // namespace transferase
