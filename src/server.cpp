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
#include "logger.hpp"
#include "utilities.hpp"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/system.hpp>  // for boost::system::error_code

#include <cassert>
#include <cerrno>
#include <csignal>  // std::raise
#include <cstdint>
#include <cstdlib>  // for std::exit, std::getenv, EXIT_SUCCESS
#include <cstring>  // std::strsignal
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>  // for std::size
#include <memory>    // std::make_shared<>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>      // for open, O_APPEND, O_CREAT, O_RDONLY
#include <sys/stat.h>   // for umask
#include <sys/types.h>  // for pid_t, mode_t
#include <syslog.h>
#include <unistd.h>

namespace transferase {

[[nodiscard]] static auto
get_daemon_filename(const std::string &filename,
                    std::error_code &ec) -> std::string {
  static constexpr auto system_dir = "/var/tmp/transferase";
  auto dirname = get_transferase_config_dir_default(ec);
  if (dirname.empty() || ec)
    // Could not get the config dir, so go with a system dir
    dirname = system_dir;
  if (!std::filesystem::is_directory(dirname, ec)) {
    const bool create_ok = std::filesystem::create_directories(dirname, ec);
    if (!create_ok || ec)
      return {};
  }
  return std::filesystem::path(dirname) / filename;
}

[[nodiscard]] static inline auto
get_daemon_stdout_filename(std::error_code &error) -> std::string {
  static constexpr auto filename = "transferase_server.log";
  return get_daemon_filename(filename, error);
}

static auto
write_pid_to_file(const std::string &pid_filename,
                  std::error_code &ec) -> void {
  auto &lgr = logger::instance();
  const auto pid_file_exists = std::filesystem::exists(pid_filename, ec);
  if (ec) {
    lgr.error("Error: {}", ec);
    return;
  }

  if (pid_file_exists) {
    ec = std::make_error_code(std::errc::file_exists);
    lgr.error("Error: pid file {} exists ({})", pid_filename, ec);
    return;
  }
  const auto pid = getpid();
  lgr.info("transferase daemon pid: {}", pid);
  std::ofstream out(pid_filename);
  if (!out) {
    ec = std::make_error_code(std::errc{errno});
    lgr.error("Error writing pid file {}: {}", pid_filename, ec);
    return;
  }
  lgr.info("transferase daemon pid file: {}", pid_filename);
  const auto pid_str = std::format("{}", pid);
  out.write(pid_str.data(), static_cast<std::streamsize>(std::size(pid_str)));
  if (!out) {
    ec = std::make_error_code(std::errc{errno});
    lgr.error("Error writing pid file {}: {}", pid_filename, ec);
    return;
  }
}

server::server(const std::string &address, const std::string &port,
               const std::uint32_t n_threads, const std::string &methylome_dir,
               const std::string &genome_index_file_dir,
               const std::uint32_t max_live_methylomes, logger &lgr,
               std::error_code &ec) :
  // io_context ios uses default constructor
  n_threads{n_threads},
#if defined(SIGQUIT)
  signals(ioc, SIGINT, SIGTERM, SIGQUIT),
#else
  signals(ioc, SIGINT, SIGTERM),
#endif
  acceptor(ioc),
  handler(methylome_dir, genome_index_file_dir, max_live_methylomes), lgr{lgr} {
  // ADS: after calling do_await_stop, must raise signal before any return
  do_await_stop();  // start waiting for signals

  boost::asio::ip::tcp::resolver resolver(ioc);
  boost::system::error_code boost_ec;
  const auto resolved = resolver.resolve(address, port, boost_ec);
  if (boost_ec) {
    ec = boost_ec;
    lgr.error("{} {}:{}", ec, address, port);
    (void)std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  assert(!resolved.empty());
  const boost::asio::ip::tcp::endpoint endpoint = *resolved.begin();
  const auto endpoint_str = boost::lexical_cast<std::string>(endpoint);
  lgr.info("Resolved endpoint {}", endpoint_str);

  // open acceptor...
  (void)acceptor.open(endpoint.protocol(), boost_ec);
  if (boost_ec) {
    ec = boost_ec;
    lgr.error("Error opening endpoint {}: {}", endpoint_str, ec);
    (void)std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  // ...with option to reuse the address (SO_REUSEADDR)
  (void)acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true),
                            boost_ec);
  if (boost_ec) {
    ec = boost_ec;
    lgr.error("Error setting SO_REUSEADDR: {}", ec);
    (void)std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  (void)acceptor.bind(endpoint, boost_ec);
  if (boost_ec) {
    ec = boost_ec;
    lgr.error("Error binding endpoint {}: {}", endpoint_str, ec);
    (void)std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  (void)acceptor.listen(boost::asio::socket_base::max_listen_connections,
                        boost_ec);
  if (boost_ec) {
    ec = boost_ec;
    lgr.error("Error listening  on endpoint {}: {}", endpoint_str, ec);
    (void)std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }
  do_accept();
}

server::server(const std::string &address, const std::string &port,
               const std::uint32_t n_threads, const std::string &methylome_dir,
               const std::string &genome_index_file_dir,
               const std::uint32_t max_live_methylomes, logger &lgr,
               std::error_code &ec, [[maybe_unused]] const bool daemonize,
               const std::string &pid_filename) :
  // io_context ioc uses default constructor
  n_threads{n_threads},
#if defined(SIGQUIT)
  // ADS: (todo) SIGHUP should re-read config file
  signals(ioc, SIGINT, SIGTERM, SIGQUIT),
#else
  signals(ioc, SIGINT, SIGTERM),
#endif
  acceptor(ioc),
  handler(methylome_dir, genome_index_file_dir, max_live_methylomes), lgr{lgr},
  pid_filename{pid_filename} {
  // ADS: standard workflow for daemonizing
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
  setsid();  // ADS: so far no warnings or errors

  // Process inherits working dir from parent. Could be on a mounted
  // filesystem, which means that the daemon would prevent filesystem
  // from being unmounted. Changing to root dir avoids this problem.
  // chdir("/");
  const int chdir_ret =
    chdir(std::filesystem::current_path().root_path().string().data());
  if (chdir_ret) {
    ec = std::make_error_code(std::errc(errno));
    lgr.error("Server failed to change directory: {}", ec);
    (void)std::raise(SIGTERM);
    return;
  }

  // File mode creation mask is inherited from parent process. We
  // don't want to restrict perms on new files, so umask is cleared.
  umask(0);

  // A second fork ensures the process cannot acquire a controlling
  // terminal.
  // ioc.notify_fork(boost::asio::io_context::fork_prepare);
  if (const pid_t pid = fork()) {
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
  if (!pid_filename.empty()) {
    write_pid_to_file(pid_filename, ec);
    // error reporting within the above function
    if (ec) {
      // ec value already set if we are here
      return;
    }
  }

  // close standard streams to decouple the daemon from the terminal
  // that started it
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  // NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)

  // We don't want the daemon to have any standard input.
  static constexpr auto dev_null = "/dev/null";
  auto open_value = open(dev_null, O_RDONLY);
  if (open_value < 0) {
    ec = std::make_error_code(std::errc(errno));
    lgr.error("Unable to open {}: {}", dev_null, ec);
    return;
  }

  // Send standard output to a log file in case something would be
  // written there.
  const auto output = get_daemon_stdout_filename(ec);
  if (ec) {
    lgr.error("Failed to set stdout for daemon {}: {}", output, ec);
    return;
  }
  const int flags = O_WRONLY | O_CREAT | O_APPEND;
  const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;  // 644
  open_value = open(output.data(), flags, mode);
  if (open_value < 0) {
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

  // NOLINTEND(cppcoreguidelines-pro-type-vararg)

  boost::asio::ip::tcp::resolver resolver(ioc);
  boost::system::error_code boost_ec;
  const auto resolved = resolver.resolve(address, port, boost_ec);
  if (boost_ec) {
    ec = boost_ec;
    lgr.error("{} {}:{}", ec, address, port);
    (void)std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  assert(!resolved.empty());
  const boost::asio::ip::tcp::endpoint endpoint = *resolved.begin();
  const auto endpoint_str = boost::lexical_cast<std::string>(endpoint);
  lgr.info("Resolved endpoint {}", endpoint_str);

  // open acceptor...
  (void)acceptor.open(endpoint.protocol(), boost_ec);
  if (boost_ec) {
    ec = boost_ec;
    lgr.error("Error opening endpoint {}: {}", endpoint_str, ec);
    (void)std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  // ...with option to reuse the address (SO_REUSEADDR)
  (void)acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true),
                            boost_ec);
  if (boost_ec) {
    ec = boost_ec;
    lgr.error("Error setting SO_REUSEADDR: {}", ec);
    (void)std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  (void)acceptor.bind(endpoint, boost_ec);
  if (boost_ec) {
    ec = boost_ec;
    lgr.error("Error binding endpoint {}: {}", endpoint_str, ec);
    (void)std::raise(SIGTERM);
    return;  // don't wait for signal handler
  }

  (void)acceptor.listen(boost::asio::socket_base::max_listen_connections,
                        boost_ec);
  if (boost_ec) {
    ec = boost_ec;
    lgr.error("Error listening  on endpoint {}: {}", endpoint_str, ec);
    (void)std::raise(SIGTERM);
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
  std::vector<std::jthread> threads;
  for (std::uint32_t i = 0; i < n_threads; ++i)
    threads.emplace_back([this] { ioc.run(); });  // NOLINT
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
        std::make_shared<connection>(std::move(socket), handler, lgr,
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
    [this](const boost::system::error_code boost_ec, const int signo) {
      lgr.warning("Received signal {} ({})", strsignal(signo), boost_ec);
      const auto message = std::format("Daemon stopped (pid: {})", getpid());
      // NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)
      syslog(LOG_INFO | LOG_USER, "%s", message.data());
      // NOLINTEND(cppcoreguidelines-pro-type-vararg)
      lgr.info(message);
      std::error_code ec{};
      const auto pid_file_exists = std::filesystem::exists(pid_filename, ec);
      if (ec)
        lgr.info("Error identifying PID file {}: {}", pid_filename, ec);
      if (pid_file_exists) {
        const bool remove_ok = std::filesystem::remove(pid_filename, ec);
        if (remove_ok)
          lgr.info("Removed pid file: {}", pid_filename);
      }
      // stop server by cancelling all outstanding async ops; when all
      // have finished, the call to io_context::run() will finish
      ioc.stop();
    });
}

}  // namespace transferase
