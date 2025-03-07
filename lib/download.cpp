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

#include "indicators/indicators.hpp"

#include "httplib/httplib.h"

#include <cerrno>
#include <chrono>
#include <cstdint>    // for std::uint64_t
#include <exception>  // for std::exception_ptr
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>  // for std::ref
#include <iterator>    // for std::cbegin
#include <limits>      // for std::numeric_limits
#include <print>
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>  // IWYU pragma: keep
#include <unordered_map>
#include <utility>  // for std::move

namespace transferase {

struct download_progress {
  static constexpr auto bar_width = 50u;
  indicators::ProgressBar bar;
  double total_size{};
  std::uint32_t prev_percent{};
  explicit download_progress(const std::string &filename) :
    bar{
      // clang-format off
      indicators::option::BarWidth{bar_width},
      indicators::option::Start{"["},
      indicators::option::Fill{"="},
      indicators::option::Lead{"="},
      indicators::option::Remainder{"-"},
      indicators::option::End{"]"},
      indicators::option::PostfixText{},
      // clang-format on
    } {
    const auto label = std::filesystem::path(filename).filename().string();
    bar.set_option(
      indicators::option::PostfixText{std::format("Downloading: {}", label)});
  }
  auto
  update(std::uint64_t len, std::uint64_t total) -> void {
    const std::uint32_t percent = 100.0 * static_cast<double>(len) / total;
    if (percent > prev_percent) {
      bar.set_progress(percent);
      prev_percent = percent;
    }
  }
};

auto
do_download(auto &cli, const download_request &dr, const std::string &outfile)
  -> std::tuple<std::unordered_map<std::string, std::string>, std::error_code> {

  download_progress bar(outfile);

  std::string body;

  cli.set_tcp_nodelay(true);
  auto res = cli.Get(
    dr.target,
    [&](const char *data, std::size_t data_length) {
      body.append(data, data_length);
      return true;
    },
    [&](const std::uint64_t len, const std::uint64_t total) {
      if (dr.show_progress)
        bar.update(len, total);
      return true;  // return 'false' if you want to cancel the request.
    });
  if (!res)
    return {{}, std::make_error_code(std::errc::invalid_argument)};

  std::unordered_map<std::string, std::string> fixed_headers;
  for (const auto &h : res->headers)
    fixed_headers.insert(h);

  std::ofstream out(outfile);
  if (!out)
    return {{}, std::make_error_code(std::errc(errno))};

  out.write(body.data(), std::size(body));

  return {fixed_headers, std::error_code{}};
}

[[nodiscard]]
auto
download(const download_request &dr)
  -> std::tuple<std::unordered_map<std::string, std::string>, std::error_code> {
  namespace fs = std::filesystem;
  const auto outdir = fs::path{dr.outdir};
  const auto outfile = outdir / fs::path(dr.target).filename();

  {
    std::error_code out_ec;
    if (fs::exists(outdir) && !fs::is_directory(outdir)) {
      out_ec = std::make_error_code(std::errc::file_exists);
      std::println(R"(Error validating output directory "{}": {})",
                   outdir.string(), out_ec.message());
      return {{}, out_ec};
    }
    if (!fs::exists(outdir)) {
      const bool made_dir = fs::create_directories(outdir, out_ec);
      if (!made_dir) {
        std::println(R"(Error output directory does not exist "{}": {})",
                     outdir.string(), out_ec.message());
        return {{}, out_ec};
      }
    }
    std::ofstream out_test(outfile);
    if (!out_test) {
      out_ec = std::make_error_code(std::errc(errno));
      std::println(R"(Error validating output file: "{}": {})",
                   outfile.string(), out_ec.message());
      return {{}, out_ec};
    }
    const bool remove_ok = fs::remove(outfile, out_ec);
    if (!remove_ok)
      return {{}, out_ec};
  }

  httplib::Headers headers;

  if (dr.port == "443") {
    httplib::SSLClient cli(dr.host);
    return do_download(cli, dr, outfile);
  }
  else {  // if (dr.port == "80") {
    httplib::Client cli(dr.host);
    return do_download(cli, dr, outfile);
  }
}

/// Get the timestamp for a remote file
[[nodiscard]]
auto
get_timestamp(const download_request &dr)
  -> std::chrono::time_point<std::chrono::file_clock> {
  static constexpr auto http_time_format = "%a, %d %b %Y %T %z";

  httplib::Headers headers;

  if (dr.port == "443") {
    httplib::SSLClient cli(dr.host);
    auto res = cli.Head(dr.target);
    if (!res)
      return {};
    headers = std::move(res->headers);
  }
  else {  // if (dr.port == "80") {
    httplib::Client cli(dr.host);
    auto res = cli.Head(dr.target);
    if (!res)
      return {};
    headers = std::move(res->headers);
  }
  const auto last_modified_itr = headers.find("Last-Modified");
  if (last_modified_itr == std::cend(headers))
    return {};
  std::istringstream is{last_modified_itr->second};
  std::chrono::time_point<std::chrono::file_clock> tp;
  if (!(is >> std::chrono::parse(http_time_format, tp)))
    return {};

  return tp;
}

}  // namespace transferase
