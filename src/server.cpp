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

#include <csignal>  // std::raise
#include <cstdint>
#include <cstring>  // strsignal
#include <filesystem>
#include <format>
#include <memory>  // std::make_shared<>
#include <print>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <syslog.h>
#include <unistd.h>

using std::format;
using std::jthread;
using std::make_shared;
using std::println;
using std::string;
using std::uint32_t;
using std::vector;

namespace asio = boost::asio;
namespace bs = boost::system;
namespace ip = asio::ip;
namespace fs = std::filesystem;

using tcp = ip::tcp;

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
  boost::system::error_code resolver_ec;
  const auto resolved = resolver.resolve(address, port, resolver_ec);
  if (resolver_ec) {
    lgr.error("{} {}:{}", resolver_ec, address, port);
    std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  assert(!resolved.empty());
  const tcp::endpoint endpoint = *resolved.begin();
  lgr.info("Resolved endpoint {}", boost::lexical_cast<string>(endpoint));

  // open acceptor...
  boost::system::error_code acceptor_ec;
  acceptor.open(endpoint.protocol(), acceptor_ec);
  if (acceptor_ec) {
    lgr.error("Error opening endpoint {}: {}",
              boost::lexical_cast<string>(endpoint), acceptor_ec);
    std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  // ...with option to reuse the address (SO_REUSEADDR)
  acceptor.set_option(tcp::acceptor::reuse_address(true), acceptor_ec);
  if (acceptor_ec) {
    lgr.error("Error setting SO_REUSEADDR: {}", acceptor_ec);
    std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  acceptor.bind(endpoint, acceptor_ec);
  if (acceptor_ec) {
    lgr.error("Error binding endpoint {}: {}",
              boost::lexical_cast<string>(endpoint), acceptor_ec);
    std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  acceptor.listen(asio::socket_base::max_listen_connections, acceptor_ec);
  if (acceptor_ec) {
    lgr.error("Error listening  on endpoint {}: {}",
              boost::lexical_cast<string>(endpoint), acceptor_ec);
    std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }
  do_accept();
}

server::server(const string &address, const string &port,
               const uint32_t n_threads, const string &methylome_dir,
               const uint32_t max_live_methylomes, logger &lgr,
               std::error_code &ec) :
  n_threads{n_threads},
#if defined(SIGQUIT)
  signals(ioc, SIGINT, SIGTERM, SIGQUIT),
#else
  signals(ioc, SIGINT, SIGTERM),
#endif
  acceptor(ioc), handler(methylome_dir, max_live_methylomes), lgr{lgr} {
  // io_context ios uses default constructor

  // ADS: already exists as "ioc"
  // boost::asio::io_context io_context;

  // Initialise server before becoming a daemon. If process is started
  // from a shell, this means errors will be reported back to user.

  // ADS: already done; we are here!

  // udp_daytime_server server(io_context);

  do_daemon_await_stop();  // start waiting for signals

  // ^^ ADS: above

  // Register signal handlers so that the daemon may be shut down. You may
  // also want to register for other signals, such as SIGHUP to trigger a
  // re-read of a configuration file.

  // boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
  // signals.async_wait([&](boost::system::error_code /*ec*/, int /*signo*/) {
  //   io_context.stop();
  // });

  // Inform the io_context that we are about to become a daemon. The
  // io_context cleans up any internal resources, such as threads, that may
  // interfere with forking.

  // ADS: we are about to fork; clean up threads (what else?)
  ioc.notify_fork(boost::asio::io_context::fork_prepare);

  // Fork the process and have the parent exit. If the process was started
  // from a shell, this returns control to the user. Forking a new process is
  // also a prerequisite for the subsequent call to setsid().
  if (const pid_t pid = fork()) {
    if (pid > 0) {
      // We're in the parent process and need to exit.
      //
      // When the exit() function is used, the program terminates without
      // invoking local variables' destructors. Only global variables are
      // destroyed. As the io_context object is a local variable, this means
      // we do not have to call:
      //
      //   io_context.notify_fork(boost::asio::io_context::fork_parent);
      //
      // However, this line should be added before each call to exit() if
      // using a global io_context object. An additional call:
      //
      //   io_context.notify_fork(boost::asio::io_context::fork_prepare);
      //
      // should also precede the second fork().
      std::exit(EXIT_SUCCESS);
    }
    else {
      ec = std::make_error_code(std::errc(errno));
      syslog(LOG_ERR | LOG_USER, "%s",
             format("First fork failed: {}", ec).data());
      return;
    }
  }

  // Make the process a new session leader. This detaches it from the
  // terminal.
  setsid();

  // Process inherits working dir from parent. Could be on a mounted
  // filesystem, which means that the daemon would prevent filesystem
  // from being unmounted. Changing to root dir avoids this problem.

  // chdir("/");
  chdir(fs::current_path().root_path().string().data());

  // File mode creation mask is inherited from parent process. Don't
  // want to restrict perms on created files, so the mask is cleared.
  umask(0);

  // A second fork ensures the process cannot acquire a controlling terminal.
  if (pid_t pid = fork()) {
    if (pid > 0) {
      std::exit(EXIT_SUCCESS);
    }
    else {
      ec = std::make_error_code(std::errc(errno));
      syslog(LOG_ERR | LOG_USER, "%s",
             format("Second fork failed: {}", ec).data());
      return;
    }
  }

  // Close the standard streams. This decouples the daemon from the terminal
  // that started it.
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  // We don't want the daemon to have any standard input.
  static constexpr auto dev_null = "/dev/null";
  if (open(dev_null, O_RDONLY) < 0) {
    ec = std::make_error_code(std::errc(errno));
    const auto msg = format("Unable to open {}: {}", dev_null, ec);
    syslog(LOG_ERR | LOG_USER, "%s", msg.data());
    return;
  }

  // Send standard output to a log file.
  const char *output = "/tmp/mc16.daemon.out";
  const int flags = O_WRONLY | O_CREAT | O_APPEND;
  const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  if (open(output, flags, mode) < 0) {
    ec = std::make_error_code(std::errc(errno));
    const auto msg = format("Unable to open output file {}: {}", output, ec);
    syslog(LOG_ERR | LOG_USER, "%s", msg.data());
    return;
  }

  // Also send standard error to the same log file.
  if (dup(1) < 0) {
    ec = std::make_error_code(std::errc(errno));
    const auto msg = format("Unable to dup output descriptor: {}", ec);
    syslog(LOG_ERR | LOG_USER, "%s", msg.data());
    return;
  }

  // Inform the io_context that we have finished becoming a daemon. The
  // io_context uses this opportunity to create any internal file descriptors
  // that need to be private to the new process.
  // io_context.notify_fork(boost::asio::io_context::fork_child);

  ioc.notify_fork(boost::asio::io_context::fork_child);

  // io_context 'ioc' can now be used normally
  syslog(LOG_INFO | LOG_USER, "Daemon started.");

  tcp::resolver resolver(ioc);
  boost::system::error_code resolver_ec;
  const auto resolved = resolver.resolve(address, port, resolver_ec);
  if (resolver_ec) {
    lgr.error("{} {}:{}", resolver_ec, address, port);
    std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  assert(!resolved.empty());
  const tcp::endpoint endpoint = *resolved.begin();
  lgr.info("Resolved endpoint {}", boost::lexical_cast<string>(endpoint));

  // open acceptor...
  boost::system::error_code acceptor_ec;
  acceptor.open(endpoint.protocol(), acceptor_ec);
  if (acceptor_ec) {
    lgr.error("Error opening endpoint {}: {}",
              boost::lexical_cast<string>(endpoint), acceptor_ec);
    std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  // ...with option to reuse the address (SO_REUSEADDR)
  acceptor.set_option(tcp::acceptor::reuse_address(true), acceptor_ec);
  if (acceptor_ec) {
    lgr.error("Error setting SO_REUSEADDR: {}", acceptor_ec);
    std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  acceptor.bind(endpoint, acceptor_ec);
  if (acceptor_ec) {
    lgr.error("Error binding endpoint {}: {}",
              boost::lexical_cast<string>(endpoint), acceptor_ec);
    std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  acceptor.listen(asio::socket_base::max_listen_connections, acceptor_ec);
  if (acceptor_ec) {
    lgr.error("Error listening  on endpoint {}: {}",
              boost::lexical_cast<string>(endpoint), acceptor_ec);
    std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }
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
  for (uint32_t i = 0; i < n_threads; ++i)
    threads.emplace_back([this] { ioc.run(); });
}

auto
server::do_accept() -> void {
  acceptor.async_accept(
    asio::make_strand(ioc),  // ADS: make a strand with the io_context
    [this](const boost::system::error_code ec, tcp::socket socket) {
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
  signals.async_wait(
    [this](const boost::system::error_code ec, const int signo) {
      lgr.error("Received signal {} ({})", strsignal(signo), ec);
      // stop server by cancelling all outstanding async ops; when all
      // have finished, the call to io_context::run() will finish
      ioc.stop();
    });
}

auto
server::do_daemon_await_stop() -> void {
  // capture brings 'this' into search for names
  signals.async_wait(
    [this](const boost::system::error_code ec, const int signo) {
      syslog(LOG_INFO | LOG_USER, "Daemon stopped.");
      lgr.error("Received signal {} ({})", strsignal(signo), ec);
      // stop server by cancelling all outstanding async ops; when all
      // have finished, the call to io_context::run() will finish
      ioc.stop();
    });
}
