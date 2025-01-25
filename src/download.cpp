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

#include "download.hpp"

// Can't silence IWYU on these
#include <boost/core/detail/string_view.hpp>
#include <boost/intrusive/detail/list_iterator.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/optional/optional.hpp>
#include <boost/system.hpp>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>  // IWYU pragma: keep
#include <boost/beast.hpp>

#include <cerrno>
#include <chrono>
#include <cstdint>    // for std::uint64_t
#include <exception>  // for std::exception_ptr
#include <filesystem>
#include <fstream>
#include <functional>  // for std::ref
#include <iterator>    // for std::cbegin
#include <limits>      // for std::numeric_limits
#include <print>
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <utility>  // for std::move

namespace transferase {

/// Download the header for a remote file
[[nodiscard]]
auto
get_header(const download_request &dr)
  -> std::tuple<std::unordered_map<std::string, std::string>, std::error_code> {
  // ADS: this function just gets the timestamp on a remote file.
  static constexpr auto http_version{11};

  // setup for io
  boost::asio::io_context ioc;
  boost::asio::ip::tcp::resolver resolver(ioc);
  boost::beast::tcp_stream stream(ioc);

  // lookup server endpoint
  boost::system::error_code ec{};
  const auto results = resolver.resolve(dr.host, dr.port, ec);
  if (ec)
    return {{}, std::error_code{ec}};

  stream.connect(results);

  // build the HEAD request for the target
  boost::beast::http::request<boost::beast::http::empty_body> req{
    // clang-format off
    boost::beast::http::verb::head,
    dr.target,
    http_version,
    // clang-format on
  };
  req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
  req.set(boost::beast::http::field::host, dr.host);

  boost::beast::http::write(stream, req, ec);
  if (ec)
    return {{}, std::error_code{ec}};

  // empty body to only get header
  boost::beast::http::response_parser<boost::beast::http::empty_body> p;
  p.skip(true);  // inform parser there will be no body

  boost::beast::flat_buffer buffer;
  boost::beast::http::read(stream, buffer, p, ec);
  if (ec)
    return {{}, std::error_code{ec}};

  const auto response = p.release();

  const auto make_string = [](const auto &x) {
    return (std::ostringstream() << x).str();
  };

  std::unordered_map<std::string, std::string> header = {
    {"Status", std::to_string(response.result_int())},
    {"Reason", make_string(response.result())},
  };
  for (const auto &h : response.base())
    header.emplace(make_string(h.name_string()), make_string(h.value()));

  return {header, ec};
}

auto
do_download(const download_request &dr, const std::string &outfile,
            boost::asio::io_context &ioc,
            std::unordered_map<std::string, std::string> &header,
            boost::beast::error_code &ec,
            const boost::asio::yield_context &yield) {
  // ADS: this is the function that does the downloading called from
  // as asio io context. Also, look at these constants if bugs happen
  static constexpr auto http_version{11};

  // attempt to open the file for output; do this prior to any async
  // ops starting
  boost::beast::http::file_body::value_type body;
  body.open(outfile.data(), boost::beast::file_mode::write, ec);
  if (ec)
    return;

  boost::asio::ip::tcp::resolver resolver(ioc);
  boost::beast::tcp_stream stream(ioc);

  // lookup server endpoint
  const auto results = resolver.async_resolve(dr.host, dr.port, yield[ec]);
  if (ec)
    return;

  // set the timeout for connecting
  stream.expires_after(dr.get_connect_timeout());

  // connect to server
  stream.async_connect(results, yield[ec]);
  if (ec)
    return;

  // Set up an HTTP GET request message
  boost::beast::http::request<boost::beast::http::string_body> req{
    // clang-format off
    boost::beast::http::verb::get,
    dr.target,
    http_version,
    // clang-format on
  };
  req.set(boost::beast::http::field::host, dr.host);
  req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);

  // set the timeout for download (need a message for this)
  stream.expires_after(dr.get_download_timeout());

  // send request to server
  boost::beast::http::async_write(stream, req, yield[ec]);
  if (ec)
    return;

  // make the response using the file-body
  boost::beast::http::response<boost::beast::http::file_body> res{
    std::piecewise_construct,
    std::make_tuple(std::move(body)),
    std::make_tuple(boost::beast::http::status::ok, http_version),
  };

  boost::beast::http::response_parser<boost::beast::http::file_body> p{
    std::move(res),
  };
  p.eager(true);
  p.body_limit(std::numeric_limits<std::uint64_t>::max());

  boost::beast::flat_buffer buffer;
  // read the http response
  boost::beast::http::async_read(stream, buffer, p, yield[ec]);
  if (ec)
    return;

  res = p.release();

  (void)stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both,
                                 ec);

  if (ec && ec != boost::beast::errc::not_connected)
    return;

  const auto make_string = [](const auto &x) {
    return (std::ostringstream() << x).str();
  };

  header.clear();
  header.emplace("Status", std::to_string(res.result_int()));
  header.emplace("Reason", make_string(res.result()));
  for (const auto &h : res.base())
    header.emplace(make_string(h.name_string()), make_string(h.value()));
}

[[nodiscard]]
auto
download(const download_request &dr)
  -> std::tuple<std::unordered_map<std::string, std::string>, std::error_code> {
  const auto outdir = std::filesystem::path{dr.outdir};
  const auto outfile = outdir / std::filesystem::path(dr.target).filename();

  std::error_code out_ec;
  {
    if (std::filesystem::exists(outdir) &&
        !std::filesystem::is_directory(outdir)) {
      out_ec = std::make_error_code(std::errc::file_exists);
      std::println("{}: {}", outdir.string(), out_ec.message());
      return {{}, out_ec};
    }
    if (!std::filesystem::exists(outdir)) {
      const bool made_dir = std::filesystem::create_directories(outdir, out_ec);
      if (!made_dir) {
        std::println("{}: {}", outdir.string(), out_ec.message());
        return {{}, out_ec};
      }
    }
    std::ofstream out_test(outfile);
    if (!out_test) {
      out_ec = std::make_error_code(std::errc(errno));
      std::println("{}: {}", outfile.string(), out_ec.message());
      return {{}, out_ec};
    }
    const bool remove_ok = std::filesystem::remove(outfile, out_ec);
    if (!remove_ok)
      return {{}, out_ec};
  }

  std::unordered_map<std::string, std::string> header;
  boost::beast::error_code ec;
  boost::asio::io_context ioc;
  boost::asio::spawn(boost::asio::make_strand(ioc),
                     std::bind(&do_download, dr, outfile, std::ref(ioc),
                               std::ref(header), std::ref(ec),
                               std::placeholders::_1),
                     // on completion, spawn will call this function
                     [](const std::exception_ptr &ex) {
                       if (ex)
                         std::rethrow_exception(ex);
                     });

  // NOTE: here is where it all happens
  ioc.run();

  if (!ec) {
    // make sure we have a status if the http had no error; "reason" is
    // deprecated since rfc7230
    const auto status_itr = header.find("Status");
    const auto reason_itr = header.find("Reason");
    if (status_itr == std::cend(header) || reason_itr == std::cend(header))
      ec = std::make_error_code(std::errc::invalid_argument);
    std::error_code filesys_ec;
    const bool outfile_exists = std::filesystem::exists(outfile, filesys_ec);
    if (filesys_ec && outfile_exists) {
      [[maybe_unused]]
      const bool remove_ok = std::filesystem::remove(outfile, filesys_ec);
    }
  }

  return {header, ec};
}

/// Get the timestamp for a remote file
[[nodiscard]]
auto
get_timestamp(const download_request &dr)
  -> std::chrono::time_point<std::chrono::file_clock> {
  static constexpr auto http_time_format = "%a, %d %b %Y %T %z";
  const auto [header, ec] = transferase::get_header(dr);
  if (ec)
    return {};
  const auto last_modified_itr = header.find("Last-Modified");
  if (last_modified_itr == std::cend(header))
    return {};
  std::istringstream is{last_modified_itr->second};
  std::chrono::time_point<std::chrono::file_clock> tp;
  if (!(is >> std::chrono::parse(http_time_format, tp)))
    return {};

  return tp;
}

}  // namespace transferase
