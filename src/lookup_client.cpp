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

#include "lookup_client.hpp"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include "cpg_index.hpp"
#include "genomic_interval.hpp"
#include "logger.hpp"
#include "methylome.hpp"
#include "request.hpp"
#include "response.hpp"
#include "utilities.hpp"

#include <array>
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>  // std::make_shared
#include <print>
#include <ranges>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using std::array;
using std::cerr;
using std::format;
using std::ofstream;
using std::print;
using std::println;
using std::string;
using std::uint32_t;
using std::vector;

namespace rg = std::ranges;
namespace vs = std::views;
namespace chrono = std::chrono;
using hr_clock = std::chrono::high_resolution_clock;

namespace asio = boost::asio;
namespace ph = boost::asio::placeholders;
namespace bs = boost::system;
namespace po = boost::program_options;
using tcp = boost::asio::ip::tcp;
using steady_timer = boost::asio::steady_timer;

// ADS: Canceling the pending async waits by changing the timer might
// be considered for avoiding. In most cases it will not be necessary
// to extend the deadline only to do some non-I/O work.

struct mc16_client {
  mc16_client(asio::io_context &io_context, const string &server,
              const string &port, request_header &req_hdr, request &req,
              logger &lgr) :
    resolver(io_context), socket(io_context), deadline{socket.get_executor()},
    req_hdr{req_hdr}, req{std::move(req)},  // move b/c req can be big
    lgr{lgr} {
    // (1) call async, (2) set deadline, (3) register check_deadline
    resolver.async_resolve(
      server, port,
      std::bind(&mc16_client::handle_resolve, this, ph::error, ph::results));
    [[maybe_unused]] const auto n_cancels =
      deadline.expires_after(chrono::seconds(read_timeout_seconds));
    deadline.async_wait(std::bind(&mc16_client::check_deadline, this));
  }

  auto handle_resolve(const bs::error_code &err,
                      const tcp::resolver::results_type &endpoints) -> void {
    if (!err) {
      asio::async_connect(
        socket, endpoints,
        std::bind(&mc16_client::handle_connect, this, ph::error));
      // ADS: set deadline after calling async op
      [[maybe_unused]] const auto n_cancels =
        deadline.expires_after(chrono::seconds(read_timeout_seconds));
    }
    else {
      lgr.debug("Error resolving server: {}", err);
      do_finish(err);
    }
  }

  void handle_connect(const bs::error_code &err) {
    [[maybe_unused]] const auto n_cancels =
      deadline.expires_at(steady_timer::time_point::max());
    if (!err) {
      lgr.debug("Connected to server: {}",
                boost::lexical_cast<string>(socket.remote_endpoint()));
      if (const auto req_hdr_compose{compose(req_buf, req_hdr)};
          !req_hdr_compose.error) {
        if (const auto req_body_compose = to_chars(
              req_hdr_compose.ptr, req_buf.data() + request_buf_size, req);
            !req_body_compose.error) {
          asio::async_write(
            socket,
            vector<asio::const_buffer>{
              asio::buffer(req_buf),
              asio::buffer(req.offsets),
            },
            std::bind(&mc16_client::handle_write_request, this, ph::error));
          [[maybe_unused]] const auto n_cancels =
            deadline.expires_after(chrono::seconds(read_timeout_seconds));
        }
        else {
          lgr.debug("Error forming request body: {}", req_body_compose.error);
          do_finish(req_body_compose.error);
        }
      }
      else {
        lgr.debug("Error forming request header: {}", req_hdr_compose.error);
        do_finish(req_hdr_compose.error);
      }
    }
    else {
      lgr.debug("Error connecting: {}", err);
      do_finish(err);
    }
  }

  auto handle_write_request(const bs::error_code &err) -> void {
    [[maybe_unused]] const auto n_cancels =
      deadline.expires_at(steady_timer::time_point::max());
    if (!err) {
      asio::async_read(
        socket, asio::buffer(resp_buf),
        asio::transfer_exactly(response_buf_size),
        std::bind(&mc16_client::handle_read_response_header, this, ph::error));
      [[maybe_unused]] const auto n_cancels =
        deadline.expires_after(chrono::seconds(read_timeout_seconds));
    }
    else {
      lgr.debug("Error writing request: {}", err);
      do_finish(err);
    }
  }

  auto handle_read_response_header(const bs::error_code &err) -> void {
    // ADS: does this go here?
    [[maybe_unused]] const auto n_cancels =
      deadline.expires_at(steady_timer::time_point::max());
    if (!err) {
      if (const auto resp_hdr_parse{parse(resp_buf, resp_hdr)};
          !resp_hdr_parse.error) {
        lgr.debug("Response header: {}", resp_hdr.summary_serial());
        do_read_counts();
      }
      else {
        println("Received error: {}", resp_hdr_parse.error);
        do_finish(resp_hdr_parse.error);
      }
    }
    else {
      lgr.debug("Error reading response header: {}", err);
      do_finish(err);
    }
  }

  auto do_read_counts() -> void {
    // ADS: this 'resize' seems to belong with the response class
    resp.counts.resize(req.n_intervals);
    asio::async_read(socket, asio::buffer(resp.counts),
                     asio::transfer_exactly(resp.get_counts_n_bytes()),
                     std::bind(&mc16_client::do_finish, this, ph::error));
    [[maybe_unused]] const auto n_cancels =
      deadline.expires_after(chrono::seconds(read_timeout_seconds));
  }

  auto do_finish(const std::error_code &err) -> void {
    // same consequence as canceling
    [[maybe_unused]] const auto n_cancels =
      deadline.expires_at(steady_timer::time_point::max());
    if (!err) {
      lgr.debug("Completing transaction: {}", err);
    }
    else {
      lgr.debug("Completing with error: {}", err);
    }
    status = err;
    bs::error_code shutdown_ec;  // for non-throwing
    socket.shutdown(tcp::socket::shutdown_both, shutdown_ec);
    bs::error_code socket_close_ec;  // for non-throwing
    socket.close(socket_close_ec);
  }

  auto check_deadline() -> void {
    if (!socket.is_open())  // ADS: when can this happen?
      return;

    if (const auto right_now = steady_timer::clock_type::now();
        deadline.expiry() <= right_now) {
      // deadline passed: close socket so remaining async ops are
      // cancelled (see below)
      const auto delta =
        chrono::duration_cast<chrono::seconds>(right_now - deadline.expiry());
      lgr.debug("Error deadline expired by: {}", delta.count());

      bs::error_code shutdown_ec;  // for non-throwing
      socket.shutdown(tcp::socket::shutdown_both, shutdown_ec);
      lgr.debug("Shutdown status: {}", shutdown_ec);
      [[maybe_unused]] const auto n_cancels =
        deadline.expires_at(steady_timer::time_point::max());

      /* ADS: closing here if needed?? */
      bs::error_code socket_close_ec;  // for non-throwing
      socket.close(socket_close_ec);
    }

    // wait again
    deadline.async_wait(std::bind(&mc16_client::check_deadline, this));
  }

  tcp::resolver resolver;
  tcp::socket socket;
  steady_timer deadline;
  request_buffer req_buf;
  request_header req_hdr;
  request req;
  response_buffer resp_buf;
  response_header resp_hdr;
  response resp;
  std::error_code status;
  logger &lgr;
  uint32_t read_timeout_seconds{3};
};  // struct mc16_client

static auto
log_debug_index(const cpg_index &index) {
  static constexpr string delim = "\n";
  logger &lgr = logger::instance();
  for (const auto word : vs::split(format("{}", index), delim))
    lgr.debug("cpg_index: {}", std::string_view(word));
}

auto
lookup_client_main(int argc, char *argv[]) -> int {
  static constexpr mc16_log_level default_log_level{mc16_log_level::warning};
  static constexpr auto default_port = "5000";
  static const auto description = "lookup-client";

  string port{};
  string accession{};
  string index_file{};
  string intervals_file{};
  string hostname{};
  string output_file{};
  mc16_log_level log_level{};

  po::options_description desc(description);
  desc.add_options()
    // clang-format off
    ("help,h", "produce help message")
    ("hostname,H", po::value(&hostname)->required(), "hostname")
    ("port,p", po::value(&port)->default_value(default_port), "port")
    ("accession,a", po::value(&accession)->required(), "accession")
    ("index,x", po::value(&index_file)->required(), "index file")
    ("intervals,i", po::value(&intervals_file)->required(), "intervals file")
    ("output,o", po::value(&output_file)->required(), "output file")
    ("log-level", po::value(&log_level)->default_value(default_log_level),
     "log level {debug,info,warning,error}")
    // clang-format on
    ;
  try {
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help")) {
      desc.print(std::cout);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch (po::error &e) {
    println(cerr, "{}", e.what());
    desc.print(cerr);
    return EXIT_FAILURE;
  }

  logger &lgr = logger::instance(
    std::make_shared<std::ostream>(std::cout.rdbuf()), description, log_level);
  if (!lgr) {
    println("Failure initializing logging: {}.", lgr.get_status());
    return EXIT_FAILURE;
  }

  lgr.info("Arguments");
  lgr.info("Accession: {}", accession);
  lgr.info("Hostname: {}", hostname);
  lgr.info("Port: {}", port);
  lgr.info("Index file: {}", index_file);
  lgr.info("Intervals file: {}", intervals_file);
  lgr.info("Output file: {}", output_file);

  cpg_index index{};
  if (index.read(index_file) != 0) {
    lgr.error("Failed to read cpg index: {}", index_file);
    return EXIT_FAILURE;
  }

  if (log_level == mc16_log_level::debug)
    log_debug_index(index);

  const auto gis = genomic_interval::load(index, intervals_file);
  if (gis.empty()) {
    lgr.error("Error reading intervals file: {}", intervals_file);
    return EXIT_FAILURE;
  }
  lgr.info("Number of intervals: {}", size(gis));

  const auto get_offsets_start{hr_clock::now()};
  const vector<methylome::offset_pair> offsets = index.get_offsets(gis);
  const auto get_offsets_stop{hr_clock::now()};
  lgr.debug("Elapsed time to get offsets: {:.3}s",
            duration(get_offsets_start, get_offsets_stop));

  request_header hdr{accession, index.n_cpgs_total, 0};
  request req{static_cast<uint32_t>(size(offsets)), offsets};

  asio::io_context io_context;
  mc16_client mc16c(io_context, hostname, port, hdr, req, lgr);
  const auto client_start{hr_clock::now()};
  io_context.run();
  const auto client_stop{hr_clock::now()};

  lgr.debug("Elapsed time for query: {:.3}s",
            duration(client_start, client_stop));

  if (mc16c.status) {
    lgr.error("Transaction failed: {}", mc16c.status);
    return EXIT_FAILURE;
  }
  else
    lgr.info("Transaction status: {}", mc16c.status);

  std::ofstream out(output_file);
  if (!out) {
    lgr.error("failed to open output file: {}", output_file);
    return EXIT_FAILURE;
  }

  const auto output_start{hr_clock::now()};
  write_intervals(out, index, gis, mc16c.resp.counts);
  const auto output_stop{hr_clock::now()};
  lgr.debug("Elapsed time for output: {:.3}s",
            duration(output_start, output_stop));

  return EXIT_SUCCESS;
}
