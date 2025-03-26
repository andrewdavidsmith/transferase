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

#ifndef LIB_SERVER_HPP_
#define LIB_SERVER_HPP_

#include "request_handler.hpp"

#include "asio.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <system_error>

namespace transferase {

class logger;

struct server {
  server(const server &) = delete;
  server &
  operator=(const server &) = delete;

  server(const std::string &address, const std::string &port,
         const std::uint32_t n_threads, const std::string &methylome_dir,
         const std::string &index_file_dir,
         const std::uint32_t max_live_methylomes, logger &lgr,
         std::error_code &ec);

  server(const std::string &address, const std::string &port,
         const std::uint32_t n_threads, const std::string &methylome_dir,
         const std::string &index_file_dir,
         const std::uint32_t max_live_methylomes, logger &lgr,
         std::error_code &ec, [[maybe_unused]] const bool daemonize,
         const std::string &pid_filename);

  // clang-format off
  auto run() -> void;
  auto do_accept() -> void;  // do async accept operation
  auto do_await_stop() -> void;  // wait for request to stop server
  auto do_daemon_await_stop() -> void;  // same but for daemon mode
  // clang-format on

  std::uint32_t n_threads{};
  asio::io_context ioc;              // performs async ops
  asio::signal_set signals;          // registers termination signals
  asio::ip::tcp::acceptor acceptor;  // listens for connections
  request_handler handler;           // handles incoming requests
  logger &lgr;
  std::atomic_uint32_t connection_id{};  // incremented per thread
  std::string pid_filename;
};

}  // namespace transferase

#endif  // LIB_SERVER_HPP_
