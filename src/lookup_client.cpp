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
#include "methylome.hpp"
#include "request.hpp"
#include "response.hpp"
#include "status_code.hpp"
#include "utilities.hpp"

#include <array>
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <string>
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
using hr_clock = std::chrono::high_resolution_clock;

namespace asio = boost::asio;
namespace ph = asio::placeholders;
namespace bs = boost::system;
namespace po = boost::program_options;
using tcp = asio::ip::tcp;

struct mc16_client {
  mc16_client(asio::io_context &io_context, const string &server,
              const string &port, request_header &req_hdr, request &req,
              bool verbose = false) :
    resolver(io_context), socket(io_context),
    req_hdr{req_hdr},
    req{std::move(req)},  // part might be big
    verbose{verbose} {
    resolver.async_resolve(
      server, port,
      std::bind(&mc16_client::handle_resolve, this, ph::error, ph::results));
  }

  auto handle_resolve(const bs::error_code &err,
                      const tcp::resolver::results_type &endpoints) -> void {
    if (!err) {
      asio::async_connect(
        socket, endpoints,
        std::bind(&mc16_client::handle_connect, this, ph::error));
    }
    else if (verbose)
      println("Error resolving server: {}", err.message());
  }

  void handle_connect(const bs::error_code &err) {
    if (!err) {
      if (verbose)
        println("Connected to server: {}",
                boost::lexical_cast<string>(socket.remote_endpoint()));
      const auto [hdr_ptr, hdr_err] =
        to_chars(req_buf.data(), req_buf.data() + request_buf_size, req_hdr);
      // req.to_buffer(buf);  // ADS: remove copying from this?
      if (hdr_err) {
        if (verbose)
          println("Error forming request header: {}", hdr_err.message());
      }
      else {
        const auto [req_ptr, req_err] =
          to_chars(hdr_ptr, req_buf.data() + request_buf_size, req);
        if (req_err) {
          if (verbose)
            println("Error forming request: {}", req_err.message());
        }
        else {
          asio::async_write(
            socket,
            vector<asio::const_buffer>{
              asio::buffer(req_buf),
              asio::buffer(req.offsets),
            },
            std::bind(&mc16_client::handle_write_request, this, ph::error));
        }
      }
    }
    else if (verbose)
      println("Error connecting: {}", err.message());
  }

  auto handle_write_request(const bs::error_code &err) -> void {
    if (!err) {
      asio::async_read(
        socket, asio::buffer(resp_buf), asio::transfer_exactly(response_buf_size),
        std::bind(&mc16_client::handle_read_response_header, this, ph::error));
    }
    else if (verbose)
      println("Error writing request: {}", err.message());
  }

  auto handle_read_response_header(const bs::error_code &err) -> void {
    if (!err) {
      // ADS: convert the buffer into the values
      const auto [resp_ptr, resp_err] = from_chars(resp_buf.data(),
                                                   resp_buf.data() + response_buf_size,
                                                   resp_hdr);
      if (resp_err) {
        println("Received error: {}", resp_err);
        do_response_header_error(resp_err);  // ADS TODO: convert result into an error
      }
      else {
        if (verbose)
          println("Response header: {}", resp_hdr.summary_serial());
        do_read_counts();
      }
    }
    else if (verbose)
      println("Error reading response header: {}", err.message());
  }

  auto do_read_counts() -> void {
    // ADS: this 'resize' seems to belong with the response class
    resp.counts.resize(req.n_intervals);
    asio::async_read(socket, asio::buffer(resp.counts),
                     asio::transfer_exactly(resp.get_counts_n_bytes()),
                     std::bind(&mc16_client::do_finish, this, ph::error));
  }

  // ADS: need to move to using status codes as we probably don't
  // really care here about the bs::error_code here
  // auto do_finish(const status_code::value status) const -> void
  // auto do_finish(const bs::error_code &err) const -> void {
  auto do_finish(const bs::error_code &err) const -> void {
    if (!err) {  // status == status_code::ok
      if (verbose)
        println("Completing transaction: {}", err.message());  // status
    }
    else if (verbose)
      println("Error reading counts: {}", err.message());  // status
  }

  auto do_response_header_error(const std::error_condition &err) const -> void {
    if (!err) {  // status == status_code::ok
      if (verbose)
        println("Completing transaction: {}", err.message());  // status
    }
    else if (verbose)
      println("Error reading counts: {}", err.message());  // status
  }

  tcp::resolver resolver;
  tcp::socket socket;
  request_buffer req_buf;
  request_header req_hdr;
  request req;
  response_buffer resp_buf;
  response_header resp_hdr;
  response resp;
  bool verbose{};
};

auto
lookup_client_main(int argc, char *argv[]) -> int {
  static constexpr auto default_port = "5000";
  static const auto description = "client";

  bool verbose{};

  string port{};
  string accession{};
  string index_file{};
  string intervals_file{};
  string hostname{};
  string output_file{};

  po::options_description desc(description);
  // clang-format off
  desc.add_options()
    ("help,h", "produce help message")
    ("hostname,H", po::value(&hostname)->required(), "hostname")
    ("port,p", po::value(&port)->default_value(default_port), "port")
    ("accession,a", po::value(&accession)->required(), "accession")
    ("index,x", po::value(&index_file)->required(), "index file")
    ("intervals,i", po::value(&intervals_file)->required(), "intervals file")
    ("output,o", po::value(&output_file)->required(), "output file")
    ("verbose,v", po::bool_switch(&verbose), "print more run info")
    ;
  // clang-format on
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

  if (verbose)
    print("accession: {}\n"
          "hostname: {}\n"
          "port: {}\n"
          "index: {}\n"
          "intervals: {}\n"
          "output: {}\n",
          accession, hostname, port, index_file, intervals_file, output_file);

  cpg_index index{};
  if (index.read(index_file) != 0) {
    println(cerr, "failed to read cpg index: {}", index_file);
    return EXIT_FAILURE;
  }

  const auto gis = genomic_interval::load(index, intervals_file);
  if (gis.empty()) {
    println(cerr, "failed to read intervals file: {}", intervals_file);
    return EXIT_FAILURE;
  }
  if (verbose)
    print("n_intervals: {}\n", size(gis));

  const auto get_offsets_start{hr_clock::now()};
  const vector<methylome::offset_pair> offsets = index.get_offsets(gis);
  const auto get_offsets_stop{hr_clock::now()};
  if (verbose)
    println("Elapsed time to get offsets: {:.3}s",
            duration(get_offsets_start, get_offsets_stop));

  request_header hdr{accession, index.n_cpgs_total, 0};
  request req{static_cast<uint32_t>(size(offsets)), offsets};

  asio::io_context io_context;
  mc16_client mc16c(io_context, hostname, port, hdr, req, verbose);
  const auto client_start{hr_clock::now()};
  io_context.run();
  const auto client_stop{hr_clock::now()};
  if (verbose)
    println("Elapsed time for query: {:.3}s",
            duration(client_start, client_stop));

  if (verbose)
    println("Response summary:\n{}", mc16c.resp_hdr.summary());

  std::ofstream out(output_file);
  if (!out) {
    println("failed to open output file: {}", output_file);
    return EXIT_FAILURE;
  }

  const auto output_start{hr_clock::now()};
  write_intervals(out, index, gis, mc16c.resp.counts);
  const auto output_stop{hr_clock::now()};
  if (verbose)
    println("elapsed time for output: {:.3}s",
            duration(output_start, output_stop));

  return EXIT_SUCCESS;
}
