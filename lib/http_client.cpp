// SPDX-License-Identifier: MIT; Copyright 2026 Andrew D Smith

#include "http_client.hpp"
#include "http_error_code.hpp"
#include "http_header.hpp"

#include <curl/curl.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <format>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>

namespace transferase {

namespace {
struct curl_setup_teardown {
  curl_setup_teardown() {
    if (const auto r = curl_global_init(CURL_GLOBAL_DEFAULT); r != CURLE_OK)
      throw std::runtime_error(curl_easy_strerror(r));
  }
  // clang-format off
  curl_setup_teardown(const curl_setup_teardown &) = delete;
  curl_setup_teardown &operator=(const curl_setup_teardown &) = delete;
  curl_setup_teardown(curl_setup_teardown &&) noexcept = default;
  curl_setup_teardown &operator=(curl_setup_teardown &&) noexcept = default;
  // clang-format on
  ~curl_setup_teardown() { curl_global_cleanup(); }
};
}  // namespace

auto
init_download() -> void {
  static curl_setup_teardown unused;
  (void)unused;  // NOLINT
}

[[nodiscard]] static auto
download_http(const std::string &url, const std::chrono::microseconds timeout,
              const std::string &outfile = "") -> http_header {
  static constexpr auto time_buf_size = 1024;
  using curl_uptr = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
  using FILE_uptr = std::unique_ptr<std::FILE, int (*)(std::FILE *)>;
  FILE_uptr f{nullptr, &std::fclose};
  curl_uptr curl(curl_easy_init(), &curl_easy_cleanup);

  curl_easy_setopt(curl.get(), CURLOPT_FILETIME, 1);
  curl_easy_setopt(curl.get(), CURLOPT_FAILONERROR, 1);
  using std::literals::chrono_literals::operator""ms;
  if (timeout != 0ms)
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, timeout);
  if (outfile.empty())
    curl_easy_setopt(curl.get(), CURLOPT_NOBODY, 1);
  else {
    f = FILE_uptr(std::fopen(std::data(outfile), "wb"), &std::fclose);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, f.get());
  }
  curl_easy_setopt(curl.get(), CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_URL, std::data(url));
  if (const CURLcode rc = curl_easy_perform(curl.get()); rc != CURLE_OK)
    throw std::runtime_error(curl_easy_strerror(rc));

  http_header hdr;
  std::size_t http_status_code{};
  if (const auto r = curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE,
                                       &http_status_code);
      r != CURLE_OK)
    throw std::runtime_error(curl_easy_strerror(r));
  hdr.status_code = std::to_string(http_status_code);
  curl_off_t sz{};
  if (const auto r =
        curl_easy_getinfo(curl.get(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &sz);
      r != CURLE_OK)
    throw std::runtime_error(curl_easy_strerror(r));
  hdr.content_length = sz;
  std::time_t ts{};
  if (const auto r = curl_easy_getinfo(curl.get(), CURLINFO_FILETIME_T, &ts);
      r != CURLE_OK)
    throw std::runtime_error(curl_easy_strerror(r));

  auto buf = std::array<char, time_buf_size>{};
  struct tm tm{};
  std::ignore = localtime_r(&ts, &tm);  // localtime_r is thread-safe
  if (!std::strftime(std::data(buf), std::size(buf), "%Y-%m-%d %H:%M:%S", &tm))
    throw std::runtime_error("failed to format time from http header");
  hdr.last_modified = std::string(std::data(buf));
  return hdr;
}

static constexpr auto
get_protocol(const auto &port) {
  return port == "443" ? "https" : "http";
}

[[nodiscard]] auto
download_http(const std::string &host, const std::string &port,
              const std::string &target, const std::string &outfile,
              const std::chrono::microseconds timeout)
  -> std::tuple<http_header, std::error_code> {
  init_download();
  const auto url = std::format("{}://{}/{}", get_protocol(port), host, target);
  auto header = download_http(url, timeout, outfile);
  return {header, http_error_code{}};
}

[[nodiscard]] auto
download_header_http(const std::string &host, const std::string &port,
                     const std::string &target,
                     const std::chrono::microseconds timeout) -> http_header {
  init_download();
  const auto url = std::format("{}://{}/{}", get_protocol(port), host, target);
  auto header = download_http(url, timeout);
  return header;
}

}  // namespace transferase
