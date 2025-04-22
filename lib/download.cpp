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

#include "http_client.hpp"
#include "http_header.hpp"

#include <cerrno>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <print>    // IWYU pragma: keep
#include <sstream>  // IWYU pragma: keep
#include <string>
#include <system_error>
#include <tuple>  // IWYU pragma: keep
#include <unordered_map>

namespace transferase {

[[nodiscard]]
auto
download(const download_request &dr)
  -> std::tuple<std::unordered_map<std::string, std::string>, std::error_code> {
  namespace fs = std::filesystem;
  const auto outdir = fs::path{dr.outdir};
  const auto outfile = outdir / fs::path(dr.target).filename();

  std::unordered_map<std::string, std::string> m;

  {
    std::error_code out_ec;
    if (fs::exists(outdir) && !fs::is_directory(outdir)) {
      out_ec = std::make_error_code(std::errc::file_exists);
      return {m, out_ec};
    }
    if (!fs::exists(outdir)) {
      const bool made_dir = fs::create_directories(outdir, out_ec);
      if (!made_dir)
        return {m, out_ec};
    }
    std::ofstream out_test(outfile);
    if (!out_test) {
      out_ec = std::make_error_code(std::errc(errno));
      return {m, out_ec};
    }
    const bool remove_ok = fs::remove(outfile, out_ec);
    if (!remove_ok)
      return {m, out_ec};
  }

  const auto [header, error] =
    download_http(dr.host, dr.port, dr.target, outfile, dr.download_timeout,
                  dr.show_progress);

  if (error)
    return {m, error};

  m.emplace("status", header.status_code);
  m.emplace("last-modified", header.last_modified);
  m.emplace("content-length", std::to_string(header.content_length));

  return {m, error};
}

/// Get the timestamp for a remote file
[[nodiscard]]
auto
get_timestamp(const download_request &dr)
  -> std::chrono::time_point<std::chrono::file_clock> {
  static constexpr auto http_time_format = "%a, %d %b %Y %H:%M:%S GMT";
  const auto header =
    download_header_http(dr.host, dr.port, dr.target, dr.download_timeout);
  struct tm t;
  strptime(header.last_modified.data(), http_time_format, &t);
  const auto stime = std::chrono::system_clock::from_time_t(std::mktime(&t));
  return std::chrono::file_clock::from_sys(stime);
}

}  // namespace transferase
