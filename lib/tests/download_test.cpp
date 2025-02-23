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

#include "unit_test_utils_cli.hpp"

#include <boost/beast.hpp>
#include <gtest/gtest.h>

#include <algorithm>  // IWYU pragma: keep
#include <chrono>
#include <filesystem>
#include <format>
#include <ranges>
#include <string>
#include <utility>

using namespace transferase;  // NOLINT

TEST(download_test, send_request_timeout) {
  const auto target = std::filesystem::path{"/delay/1"};
  const auto outdir = std::filesystem::path{"/tmp"};
  const std::chrono::milliseconds connect_timeout{0};
  const std::chrono::milliseconds download_timeout{0'500};  // 0.5s
  // clang-format off
  const download_request dr{
    "httpbin.org",
    "80",
    target,
    outdir,
    connect_timeout,
    download_timeout,
  };
  // clang-format on
  const auto expected_outfile = outdir / target.filename();

  // Simulate the download
  const auto [headers, ec] = download(dr);
  std::ignore = headers;

  EXPECT_TRUE(ec.value() == std::to_underlying(boost::beast::error::timeout));
  if (std::filesystem::exists(expected_outfile)) {
    const bool remove_ok = std::filesystem::remove(expected_outfile);
    EXPECT_TRUE(remove_ok);
  }
}

TEST(download_test, receive_download_timeout) {
  const auto target = std::filesystem::path{"/delay/1"};
  const auto outdir = std::filesystem::path{"/tmp"};
  const std::chrono::milliseconds connect_timeout{1'500};  // 1.5s
  const std::chrono::milliseconds download_timeout{1};     // 0.001s
  // clang-format off
  const download_request dr{
    "httpbin.org",
    "80",
    target,
    outdir,
    connect_timeout,
    download_timeout,
  };
  // clang-format on
  const auto expected_outfile = outdir / target.filename();

  const auto [headers, ec] = download(dr);  // do the download
  std::ignore = headers;

  EXPECT_TRUE(ec.value() == std::to_underlying(boost::beast::error::timeout));

  if (std::filesystem::exists(expected_outfile)) {
    const bool remove_ok = std::filesystem::remove(expected_outfile);
    EXPECT_TRUE(remove_ok);
  }
}

[[nodiscard]] static auto
to_string(const auto &maplike) -> std::string {
  const auto fmt_pair = [](const auto &pairlike) -> std::string {
    return std::format(R"("{}":"{}"\n)", pairlike.first, pairlike.second);
  };
  return maplike | std::views::transform(fmt_pair) |
         std::views::join_with(',') | std::ranges::to<std::string>();
}

TEST(download_test, download_non_existent_file) {
  const std::chrono::milliseconds connect_timeout{3'000};   // 3s
  const std::chrono::milliseconds download_timeout{3'000};  // 3s
  // ADS: note the prefix slash below
  const std::filesystem::path target{generate_temp_filename_cli("/file", "txt")};
  // ADS: need to make sure this will be unique; got caught with an
  // pre-existing filename
  const std::filesystem::path outdir{"/tmp"};
  // clang-format off
  const download_request dr{
    "example.com",  // host
    "80",           // port
    target,
    outdir,
    connect_timeout,
    download_timeout,
  };
  // clang-format on
  const auto expected_outfile = outdir / target.filename();

  const auto [headers, ec] = download(dr);  // do the download

  const auto bad_target =
    (ec.value() == std::to_underlying(boost::beast::http::error::bad_target));

  const auto timeout_happened =
    (ec.value() == std::to_underlying(boost::beast::error::timeout));

  // Only check things if a timeout didn't happen
  EXPECT_TRUE(timeout_happened || bad_target) << to_string(headers);
  EXPECT_TRUE(timeout_happened || headers.contains("Status"))
    << to_string(headers);
  EXPECT_TRUE(timeout_happened || headers.contains("Reason"))
    << to_string(headers);

  // randomly generated filename should not exist as a uri
  EXPECT_TRUE(timeout_happened || headers.at("Status") == "404");

  if (std::filesystem::exists(expected_outfile)) {
    const bool remove_ok = std::filesystem::remove(expected_outfile);
    EXPECT_TRUE(remove_ok);
  }
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
    ec.value() == std::to_underlying(boost::beast::error::timeout);

  // Only check things if a timeout didn't happen
  EXPECT_TRUE(timeout_happened || !ec);
  EXPECT_TRUE(timeout_happened || headers.contains("Status"));
  EXPECT_TRUE(timeout_happened || headers.contains("Reason"));

  // index.html should exist
  EXPECT_TRUE(timeout_happened || headers.at("Status") == "200");

  if (std::filesystem::exists(expected_outfile)) {
    const bool remove_ok = std::filesystem::remove(expected_outfile);
    EXPECT_TRUE(remove_ok);
  }
}
