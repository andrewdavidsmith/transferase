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

#ifndef SRC_DOWNLOAD_HPP_
#define SRC_DOWNLOAD_HPP_

#include <cstdint>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_map>

namespace xfrase {

struct download_request {
  std::string host;
  std::string port;
  std::string target;
  std::string outdir;
  std::uint32_t connect_timeout{10};    // seconds
  std::uint32_t download_timeout{240};  // seconds
  download_request(const std::string &host, const std::string &port,
                   const std::string &target, const std::string &outdir) :
    host{host}, port{port}, target{target}, outdir{outdir} {}
  download_request(const std::string &host, const std::string &port,
                   const std::string &target, const std::string &outdir,
                   const std::uint32_t connect_timeout,
                   const std::uint32_t download_timeout) :
    host{host}, port{port}, target{target}, outdir{outdir},
    connect_timeout{connect_timeout}, download_timeout{download_timeout} {}
};

[[nodiscard]]
auto
download(const download_request &dr)
  -> std::tuple<std::unordered_map<std::string, std::string>, std::error_code>;

}  // namespace xfrase

#endif  // SRC_DOWNLOAD_HPP_
