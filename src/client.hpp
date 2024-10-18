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

#ifndef SRC_CLIENT_HPP_
#define SRC_CLIENT_HPP_

#include "logger.hpp"
#include "request.hpp"
#include "request_handler.hpp"
#include "response.hpp"

#include <boost/asio.hpp>

#include <cstdint>  // std::uint32_t
#include <string>
#include <vector>

class mc16_client {
public:
  mc16_client(const std::string &server, const std::string &port,
              request_header &req_hdr, request &req, logger &lgr);

  auto run() -> std::error_code {
    io_context.run();
    return status;
  }

  auto get_counts() const -> const std::vector<counts_res> & {
    return resp.counts;
  }

private:
  auto handle_resolve(
    const boost::system::error_code &err,
    const boost::asio::ip::tcp::resolver::results_type &endpoints) -> void;

  auto handle_connect(const boost::system::error_code &err) -> void;
  auto handle_write_request(const boost::system::error_code &err) -> void;
  auto
  handle_read_response_header(const boost::system::error_code &err) -> void;
  auto do_read_counts() -> void;

  auto do_finish(const std::error_code &err) -> void;
  auto check_deadline() -> void;

  boost::asio::io_context io_context;
  boost::asio::ip::tcp::resolver resolver;
  boost::asio::ip::tcp::socket socket;
  boost::asio::steady_timer deadline;
  request_buffer req_buf;
  request_header req_hdr;
  request req;
  response_buffer resp_buf;
  response_header resp_hdr;
  response resp;
  std::error_code status;
  logger &lgr;
  std::uint32_t read_timeout_seconds{3};
};  // class mc16_client

#endif  // SRC_CLIENT_HPP_
