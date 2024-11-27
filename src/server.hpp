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

#ifndef SRC_SERVER_HPP_
#define SRC_SERVER_HPP_

#include "logger.hpp"
#include "request_handler.hpp"

#include <boost/asio.hpp>

#include <atomic>
#include <cstdint>
#include <string>
#include <system_error>

struct server {
  server(const server &) = delete;
  server &
  operator=(const server &) = delete;

  explicit server(const std::string &address, const std::string &port,
                  const std::uint32_t n_threads,
                  const std::string &methylome_dir,
                  const std::string &index_file_dir,
                  const std::uint32_t max_live_methylomes, logger &lgr);

  explicit server(const std::string &address, const std::string &port,
                  const std::uint32_t n_threads,
                  const std::string &methylome_dir,
                  const std::string &index_file_dir,
                  const std::uint32_t max_live_methylomes, logger &lgr,
                  std::error_code &ec);

  auto
  run() -> void;
  auto
  do_accept() -> void;  // do async accept operation
  auto
  do_await_stop() -> void;  // wait for request to stop server
  auto
  do_daemon_await_stop() -> void;

  std::uint32_t n_threads{};
  boost::asio::io_context ioc;              // performs async ops
  boost::asio::signal_set signals;          // registers termination signals
  boost::asio::ip::tcp::acceptor acceptor;  // listens for connections
  request_handler handler;                  // handles incoming requests
  logger &lgr;
  std::atomic_uint32_t connection_id{};  // incremented per thread
};

#endif  // SRC_SERVER_HPP_
