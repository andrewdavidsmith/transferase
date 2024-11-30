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

static auto
write_pid_to_file(std::error_code &ec) -> void {
  static const auto pid_file_rhs =
    std::filesystem::path(".config") / "mxe" / "MXE_PID_FILE";

  auto &lgr = logger::instance();

  // write the pid of the daemon to a file
  const auto env_home = std::getenv("HOME");
  if (!env_home) {
    ec = std::make_error_code(std::errc(errno));
    lgr.error("Error forming config dir: {}", ec);
    return;
  }
  const std::filesystem::path pid_file =
    std::filesystem::path(env_home) / pid_file_rhs;
  if (std::filesystem::exists(pid_file)) {
    ec = std::make_error_code(std::errc::file_exists);
    lgr.error("Error: {}", ec);
    return;
  }
  const auto pid = getpid();
  lgr.info("mxe daemon pid: {}", pid);
  std::ofstream out(pid_file);
  if (!out) {
    ec = std::make_error_code(std::errc{errno});
    lgr.error("Error writing pid file: {} ({})", ec, pid_file);
    return;
  }
  lgr.info("mxe daemon pid file: {}", pid_file);
  const auto pid_str = format("{}", pid);
  out.write(pid_str.data(), size(pid_str));
  if (!out) {
    ec = std::make_error_code(std::errc{errno});
    lgr.error("Error writing pid file: {} ({})", ec, pid_file);
    return;
  }
}

server::server(const string &address, const string &port,
               const uint32_t n_threads, const string &methylome_dir,
               const string &cpg_index_file_dir,
               const uint32_t max_live_methylomes, logger &lgr,
               std::error_code &ec) :
  // io_context ios uses default constructor
  n_threads{n_threads},
#if defined(SIGQUIT)
  signals(ioc, SIGINT, SIGTERM, SIGQUIT),
#else
  signals(ioc, SIGINT, SIGTERM),
#endif
  acceptor(ioc),
  handler(methylome_dir, cpg_index_file_dir, max_live_methylomes, ec),
  lgr{lgr} {
  // first check for errors in initializing members
  if (ec)
    return;

  // ADS: after this line we need to raise signal
  do_await_stop();  // start waiting for signals

  boost::asio::ip::tcp::resolver resolver(ioc);
  boost::system::error_code resolver_ec;
  const auto resolved = resolver.resolve(address, port, resolver_ec);
  if (resolver_ec) {
    lgr.error("{} {}:{}", resolver_ec, address, port);
    std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  assert(!resolved.empty());
  const boost::asio::ip::tcp::endpoint endpoint = *resolved.begin();
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
  acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true),
                      acceptor_ec);
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

  acceptor.listen(boost::asio::socket_base::max_listen_connections,
                  acceptor_ec);
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
               const string &cpg_index_file_dir,
               const uint32_t max_live_methylomes, logger &lgr,
               std::error_code &ec, const bool daemonize) :
  // io_context ioc uses default constructor
  n_threads{n_threads},
#if defined(SIGQUIT)
  // ADS: (todo) SIGHUP should re-read config file
  signals(ioc, SIGINT, SIGTERM, SIGQUIT),
#else
  signals(ioc, SIGINT, SIGTERM),
#endif
  acceptor(ioc),
  handler(methylome_dir, cpg_index_file_dir, max_live_methylomes, ec),
  lgr{lgr} {
  // first check for errors in initializing members
  if (ec)
    return;

  // ADS: after this line we need to raise signal
  do_daemon_await_stop();  // signals setup; start waiting for them

  // ADS: we are about to fork; clean up threads (what else?)
  ioc.notify_fork(boost::asio::io_context::fork_prepare);

  // Fork the process and have the parent exit. If the process was started
  // from a shell, this returns control to the user. Forking a new process is
  // also a prerequisite for the subsequent call to setsid().
  if (const pid_t pid = fork()) {
    if (pid > 0) {
      // in parent and need to exit
      std::exit(EXIT_SUCCESS);
    }
    else {
      ec = std::make_error_code(std::errc(errno));
      lgr.error("First fork failed: {}", ec);
      return;
    }
  }

  // make the process a new session leader, detaching it from terminal
  setsid();

  // Process inherits working dir from parent. Could be on a mounted
  // filesystem, which means that the daemon would prevent filesystem
  // from being unmounted. Changing to root dir avoids this problem.
  // chdir("/");
  chdir(std::filesystem::current_path().root_path().string().data());

  // File mode creation mask is inherited from parent process. We
  // don't want to restrict perms on new files, so umask is cleared.
  umask(0);

  // A second fork ensures the process cannot acquire a controlling
  // terminal.
  // ioc.notify_fork(boost::asio::io_context::fork_prepare);
  if (pid_t pid = fork()) {
    if (pid > 0) {
      std::exit(EXIT_SUCCESS);
    }
    else {
      ec = std::make_error_code(std::errc(errno));
      lgr.error("Second fork failed: {}", ec);
      return;
    }
  }

  // write the pid of the daemon to a file
  write_pid_to_file(ec);
  // error reporting within the above function
  if (ec)
    return;

  // close standard streams to decouple the daemon from the terminal
  // that started it
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  // We don't want the daemon to have any standard input.
  static constexpr auto dev_null = "/dev/null";
  if (open(dev_null, O_RDONLY) < 0) {
    ec = std::make_error_code(std::errc(errno));
    lgr.error("Unable to open {}: {}", dev_null, ec);
    return;
  }

  // Send standard output to a log file in case something would be
  // written there.
  // ADS: (todo) fix this hardcoded file below
  const auto output = "/tmp/mxe_daemon.out";
  const int flags = O_WRONLY | O_CREAT | O_APPEND;
  const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;  // 644
  if (open(output, flags, mode) < 0) {
    ec = std::make_error_code(std::errc(errno));
    lgr.error("Unable to open output file {}: {}", output, ec);
    return;
  }

  // Also send standard error to the same log file.
  if (dup(1) < 0) {
    ec = std::make_error_code(std::errc(errno));
    lgr.error("Unable to dup output descriptor: {}", ec);
    return;
  }

  ioc.notify_fork(boost::asio::io_context::fork_child);

  // io_context 'ioc' can now be used normally
  syslog(LOG_INFO | LOG_USER, "Daemon started.");
  lgr.info("Daemon started (pid: {})", getpid());

  boost::asio::ip::tcp::resolver resolver(ioc);
  boost::system::error_code resolver_ec;
  const auto resolved = resolver.resolve(address, port, resolver_ec);
  if (resolver_ec) {
    lgr.error("{} {}:{}", resolver_ec, address, port);
    std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  assert(!resolved.empty());
  const boost::asio::ip::tcp::endpoint endpoint = *resolved.begin();
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
  acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true),
                      acceptor_ec);
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

  acceptor.listen(boost::asio::socket_base::max_listen_connections,
                  acceptor_ec);
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
    boost::asio::make_strand(ioc),  // ADS: make a strand with the io_context
    [this](const boost::system::error_code ec,
           boost::asio::ip::tcp::socket socket) {
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
      lgr.warning("Received signal {} ({})", strsignal(signo), ec);
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
      lgr.warning("Received signal {} ({})", strsignal(signo), ec);
      const auto message = format("Daemon stopped (pid: {})", getpid());
      syslog(LOG_INFO | LOG_USER, "%s", message.data());
      lgr.info(message);
      // stop server by cancelling all outstanding async ops; when all
      // have finished, the call to io_context::run() will finish
      ioc.stop();
    });
}
