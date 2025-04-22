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

#include <download.hpp>

#include <http_error_code.hpp>
#include <macos_helper.hpp>

#include "unit_test_utils.hpp"

#include <gtest/gtest.h>

#include <algorithm>  // IWYU pragma: keep
#include <chrono>
#include <filesystem>
#include <format>
#include <ranges>
#include <string>
#include <utility>

using namespace transferase;  // NOLINT

[[nodiscard]] static auto
to_string(const auto &maplike) -> std::string {
  const auto fmt_pair = [](const auto &pairlike) -> std::string {
    return std::format("\"{}\":\"{}\"", pairlike.first, pairlike.second);
  };
  // return maplike | std::views::transform(fmt_pair) |
  //        std::views::join_with('\n') | std::ranges::to<std::string>();
  return join_with(maplike | std::views::transform(fmt_pair), '\n');
}

TEST(download_test, send_request_timeout) {
  const auto target = "/delay/1";
  const auto outdir = std::filesystem::path{"/tmp"};
  const std::chrono::microseconds timeout{1};
  // clang-format off
  download_request dr{
    "httpbin.org",
    "80",
    target,
    outdir,
  };
  dr.set_timeout(timeout);
  // clang-format on
  const auto expected_outfile = std::filesystem::path{outdir} / target;

  // Simulate the download
  const auto [headers, ec] = download(dr);

  EXPECT_TRUE(ec.value() ==
              std::to_underlying(http_error_code::inactive_timeout))
    << to_string(headers) << '\t' << "message=" << ec.message() << '\t'
    << "value=" << ec.value() << '\t' << "underlying: \""
    << http_error_code::inactive_timeout << "\"\n";
  std::error_code error;
  remove_file(expected_outfile, error);
  EXPECT_FALSE(error);
}

TEST(download_test, download_non_existent_file) {
  const std::chrono::microseconds timeout{3'000'000};  // 3s
  // ADS: note the prefix slash below
  const std::filesystem::path target{generate_temp_filename("/file", "txt")};
  // ADS: need to make sure this will be unique; got caught with an
  // pre-existing filename
  const std::filesystem::path outdir{"/tmp"};
  // clang-format off
  const download_request dr{
    "example.com",  // host
    "80",           // port
    target,
    outdir,
    timeout,
  };
  // clang-format on
  const auto expected_outfile = outdir / target.filename();

  const auto [headers, ec] = download(dr);  // do the download

  const auto timeout_happened =
    (ec.value() == std::to_underlying(http_error_code::inactive_timeout));

  // Only check things if a timeout didn't happen
  EXPECT_TRUE(timeout_happened || headers.contains("status"))
    << to_string(headers);

  // randomly generated filename should not exist as a uri
  EXPECT_TRUE(timeout_happened || headers.at("status") == "404" ||
              headers.at("status") == "400")
    << to_string(headers);

  std::error_code error;
  remove_file(expected_outfile, error);
  EXPECT_FALSE(error);
}

TEST(download_test, download_success) {
  const auto target = std::filesystem::path{"/index.html"};
  // ADS: need to make sure this will be unique; got caught with an
  // pre-existing filename
  const auto outdir = std::filesystem::path{"/tmp"};
  const download_request dr{
    "example.com",  // host
    "80",           // port
    target,
    outdir,
  };
  const auto expected_outfile = outdir / target.filename();

  const auto [headers, ec] = download(dr);  // do the download

  const auto timeout_happened =
    ec.value() == std::to_underlying(http_error_code::inactive_timeout);

  // Only check things if a timeout didn't happen
  EXPECT_TRUE(timeout_happened || !ec) << to_string(headers);
  EXPECT_TRUE(timeout_happened || headers.contains("status"))
    << to_string(headers);

  // index.html should exist
  EXPECT_TRUE(timeout_happened || headers.at("status") == "200")
    << to_string(headers);

  std::error_code error;
  remove_file(expected_outfile, error);
  EXPECT_FALSE(error);
}
