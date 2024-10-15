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
#include "connection.hpp"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>

#include <cstdint>
#include <cstring>  // strsignal
#include <memory>   // make_shared<>
#include <print>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using std::jthread;
using std::make_shared;
using std::println;
using std::string;
using std::uint32_t;
using std::vector;

namespace asio = boost::asio;
namespace bs = boost::system;
namespace ip = asio::ip;

using tcp = ip::tcp;

typedef mc16_log_level lvl;

server::server(const string &address, const string &port,
               const uint32_t n_threads, const string &methylome_dir,
               const uint32_t max_live_methylomes, logger &lgr) :
  n_threads{n_threads},
#if defined(SIGQUIT)
  signals(ioc, SIGINT, SIGTERM, SIGQUIT),
#else
  signals(ioc, SIGINT, SIGTERM),
#endif
  acceptor(ioc), handler(methylome_dir, max_live_methylomes), lgr{lgr} {
  // io_context ios uses default constructor

  do_await_stop();  // start waiting for signals

  tcp::resolver resolver(ioc);
  tcp::endpoint endpoint = *resolver.resolve(address, port).begin();

  lgr.log<lvl::info>("Server endpoint: {}",
                     boost::lexical_cast<string>(endpoint));

  // open acceptor with option to reuse the address (SO_REUSEADDR)
  acceptor.open(endpoint.protocol());
  acceptor.set_option(tcp::acceptor::reuse_address(true));
  acceptor.bind(endpoint);
  acceptor.listen();

  do_accept();
}

auto
server::run() -> void {
  /* From the docs (v1.86.0): "Multiple threads may call the run()
     function to set up a pool of threads from which the io_context
     may execute handlers. All threads that are waiting in the pool
     are equivalent and the io_context may choose any one of them to
     invoke a handler."
  */
  vector<jthread> threads;
  for (uint32_t i = 0u; i < n_threads; ++i)
    threads.emplace_back([this] { ioc.run(); });
}

auto
server::do_accept() -> void {
  acceptor.async_accept(
    asio::make_strand(ioc),  // ADS: make a strand with the io_context
    [this](const bs::error_code ec, tcp::socket socket) {
      // quit if server already stopped by signal
      if (!acceptor.is_open())
        return;
      if (!ec) {
        // ADS: accepted socket moved into connection which is started
        make_shared<connection>(std::move(socket), handler, lgr,
                                connection_id++)
          ->start();
      }
      do_accept();  // keep listening for more connections
    });
}

auto
server::do_await_stop() -> void {
  // capture brings 'this' into search for names
  signals.async_wait([this](const bs::error_code ec, const int signo) {
    lgr.log<lvl::info>("Received termination signal {} ({})", strsignal(signo),
                       ec.message());
    // stop server by cancelling all outstanding async ops; when all
    // have finished, the call to io_context::run() will finish
    ioc.stop();
  });
}
