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

#include "query_container.hpp"
#include "request_handler.hpp"
#include "response.hpp"

#include <asio.hpp>

#include <algorithm>
#include <chrono>
#include <compare>  // for operator<=
#include <string>
#include <system_error>

namespace transferase {

auto
connection::set_deadline(const std::chrono::seconds delta) -> void {
  namespace chrono = std::chrono;
  deadline = chrono::steady_clock::now() + chrono::seconds(delta);
}

[[nodiscard]] auto
connection::get_send_buf(const std::uint32_t offset) const noexcept -> const
  char * {
  const auto r = req.is_covered_request()
                   ? reinterpret_cast<const char *>(resp_cov.data())
                   : reinterpret_cast<const char *>(resp.data());
  return r + offset;  // NOLINT (*-pointer-arithmetic)
}

auto
connection::compute_intervals() noexcept -> void {
  if (req.is_covered_request())
    handler.intervals_get_levels<level_element_covered_t>(req, query, resp_hdr,
                                                          resp_cov);
  else
    handler.intervals_get_levels<level_element_t>(req, query, resp_hdr, resp);
}

auto
connection::compute_bins() noexcept -> void {
  if (req.is_covered_request())
    handler.bins_get_levels<level_element_covered_t>(req, resp_hdr, resp_cov);
  else
    handler.bins_get_levels<level_element_t>(req, resp_hdr, resp);
  if (resp_hdr.status) {
    lgr.warning("{} Error computing levels: {}", conn_id,
                resp_hdr.status.message());
    respond_with_error();
  }
  else {
    lgr.debug("{} Finished computing levels in bins", conn_id);
    respond_with_header();
  }
}

// Server logic below

auto
connection::init_read_query() -> void {
  query.resize(req.aux_value);
  query_remaining = query.get_n_bytes();
  query_byte = 0;

  read_query();
}

auto
connection::init_write_response() -> void {
  levels_remaining = get_response_size();
  levels_byte = 0;

  respond_with_levels();
}

auto
connection::read_request() -> void {
  set_deadline(comm_timeout_sec);
  auto self = shared_from_this();
  asio::async_read(
    socket, asio::buffer(req_buf), asio::transfer_exactly(request_buffer_size),
    [this, self](const std::error_code ec, auto /*n_bytes*/) {
      // transfer_exactly means no reenter so relax deadline
      set_deadline(work_timeout_sec);
      if (ec) {
        // problem reading request
        lgr.warning("{} Failed to read request: {}", conn_id, ec);
        stop();
        return;
      }
      else {
        if (const auto parse_err = parse(req_buf, req); parse_err) {
          lgr.warning("{} Request parse error: {}", conn_id, parse_err);
          resp_hdr = {parse_err, 0};
          respond_with_error();
        }
        else {
          lgr.debug("{} Received request: {}", conn_id, req.summary());
          handler.handle_request(req, resp_hdr);
          if (!resp_hdr.error()) {
            if (req.is_intervals_request()) {
              init_read_query();
            }
            else {  // req.is_bins_request()
              compute_bins();
            }
          }
          else {
            // response header already contains the error code
            respond_with_error();
          }
        }
      }
    });
  // ADS: on error: no new asyncs start; references to this
  // connection disappear; this connection gets destroyed when
  // *both* this handler returns and the timer completes; that
  // destructor destroys the socket
  // });
}

auto
connection::read_query() -> void {
  set_deadline(comm_timeout_sec);
  auto self = shared_from_this();
  socket.async_read_some(
    asio::buffer(query.data(query_byte), query_remaining),
    [this, self](const std::error_code ec,
                 const std::size_t bytes_transferred) {
      set_deadline(work_timeout_sec);
      if (!ec) {
        query_remaining -= bytes_transferred;
        query_byte += bytes_transferred;
        ++n_reads;
        max_read_size = std::max(max_read_size, bytes_transferred);
        min_read_size = std::min(min_read_size, bytes_transferred);
        if (query_remaining == 0) {
          lgr.debug("{} Finished reading query ({}B, reads={}, max={}B, "
                    "min={}B, mean={}B)",
                    conn_id, query_byte, n_reads, max_read_size, min_read_size,
                    query_byte / n_reads);
          compute_intervals();
          if (resp_hdr.status) {
            lgr.warning("{} Error computing levels: {}", conn_id,
                        resp_hdr.status.message());
            respond_with_error();
          }
          else {
            lgr.debug("{} Finished computing levels in intervals", conn_id);
            respond_with_header();
          }
        }
        else {
          read_query();
        }
      }
      else {
        lgr.warning("{} Error reading query: {}", conn_id, ec);
        resp_hdr = {request_error_code::error_reading_query, 0};
        respond_with_error();
      }
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
  else {
    set_deadline(comm_timeout_sec);
    auto self = shared_from_this();
    asio::async_write(socket, asio::buffer(resp_hdr_buf),
                      [this, self](const std::error_code ec, auto) {
                        set_deadline(work_timeout_sec);
                        if (ec)
                          lgr.error("{} Error responding: {}", conn_id, ec);
                        stop();
                      });
  }
}

auto
connection::respond_with_header() -> void {
  lgr.debug("{} Responding with header: {}", conn_id, resp_hdr.summary());
  if (const auto ec = compose(resp_hdr_buf, resp_hdr); ec) {
    lgr.error("{} Error composing response header: {}", conn_id, ec);
    stop();
    return;
  }
  else {
    set_deadline(comm_timeout_sec);
    auto self = shared_from_this();
    asio::async_write(socket, asio::buffer(resp_hdr_buf),
                      [this, self](const std::error_code ec, auto) {
                        set_deadline(work_timeout_sec);
                        if (ec) {
                          lgr.warning("{} Error sending header: {}", conn_id,
                                      ec);
                          stop();
                          return;
                        }
                        else {
                          init_write_response();
                        }
                      });
  }
}

auto
connection::respond_with_levels() -> void {
  set_deadline(comm_timeout_sec);
  auto self = shared_from_this();
  socket.async_write_some(
    asio::buffer(get_send_buf(levels_byte), levels_remaining),
    [this, self](const auto ec, const auto n_bytes) {
      if (ec) {
        lgr.warning("{} Error sending levels: {}", conn_id, ec);
        stop();
        return;
      }
      else {
        levels_remaining -= n_bytes;
        levels_byte += n_bytes;

        // ADS: collect the stats here; should be refactored
        ++n_writes;
        max_write_size = std::max(max_write_size, n_bytes);
        min_write_size = std::min(min_write_size, n_bytes);
        if (levels_remaining == 0) {
          lgr.info(
            "{} Responded with levels ({}B, writes={}, max={}B, min={}B, "
            "mean={}B)",
            conn_id, levels_byte, n_writes, max_write_size, min_write_size,
            levels_byte / n_writes);
          stop();
          return;
        }
        else {
          respond_with_levels();
        }
      }
    });
}

auto
connection::watchdog() -> void {
  auto self = shared_from_this();
  watchdog_timer.expires_at(deadline);
  watchdog_timer.async_wait([self](auto /*error*/) {
    if (!self->is_stopped()) {
      if (self->deadline <= std::chrono::steady_clock::now()) {
        self->stop();
        return;
      }
      else {
        self->watchdog();
      }
    }
  });
}

auto
connection::stop() -> void {
  lgr.debug("{} Initiating connection shutdown.", conn_id);
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
