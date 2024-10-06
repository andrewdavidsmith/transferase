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

#include "server.hpp"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>

#include <print>
#include <string>
#include <utility>
#include <thread>
#include <vector>

using std::println;
using std::string;
using std::jthread;
using std::vector;

namespace asio = boost::asio;
namespace bs = boost::system;
namespace ip = asio::ip;

using tcp = ip::tcp;

server::server(const string &address, const string &port, const uint32_t n_threads,
               const string &methylome_dir, const uint32_t max_live_methylomes,
               bool verbose)
    : verbose{verbose},
      n_threads{n_threads},
      // ioc(1),  // using default constructor
      signals(ioc),
      acceptor(ioc),
      handler(methylome_dir, max_live_methylomes, verbose) {
  signals.add(SIGINT);
  signals.add(SIGTERM);
#if defined(SIGQUIT)
  signals.add(SIGQUIT);
#endif  // defined(SIGQUIT)

  do_await_stop();  // start waiting for signals

  // ADS TODO: summarize the empty methylome set?
  // if (verbose)
  //   println("Endpoint: {}", boost::lexical_cast<string>(endpoint));

  // open acceptor with option to reuse the address (SO_REUSEADDR)
  tcp::resolver resolver(ioc);
  tcp::endpoint endpoint = *resolver.resolve(address, port).begin();
  if (verbose) println("Endpoint: {}", boost::lexical_cast<string>(endpoint));
  acceptor.open(endpoint.protocol());
  acceptor.set_option(tcp::acceptor::reuse_address(true));
  acceptor.bind(endpoint);
  acceptor.listen();

  do_accept();
}

auto
server::run() -> void {
  ioc.run();  // blocks until all async ops have finnished

  // create a pool of threads to run the io_context
  vector<jthread> threads;
  for (size_t i = 0; i < n_threads; ++i)
    threads.emplace_back([this]{ ioc.run(); });

  // // wait for all threads in the pool to exit
  // for (size_t i = 0; i < size(threads); ++i)
  //   threads[i].join();
}

auto
server::do_accept() -> void {
  // pass accepted connection to connection manager
  acceptor.async_accept([this](const bs::error_code ec, tcp::socket socket) {
    // quit if server already stopped by signal
    if (!acceptor.is_open()) return;
    if (!ec) {
      manager.start(std::make_shared<connection>(std::move(socket), manager,
                                                 handler, verbose));
    }
    do_accept();  // listen for more connections
  });
}

auto
server::do_await_stop() -> void {
  // capture brings 'this' into search for names
  signals.async_wait([this](const bs::error_code ec, const int signo) {
    if (verbose)
      println("received termination signal {} ({})", signo, ec.message());
    // stop server by cancelling all outstanding async ops; when all
    // have finished, the call to io_context::run() will finish
    acceptor.close();
    manager.stop_all();
  });
}
