// SPDX-License-Identifier: MIT; Copyright 2026 Andrew D Smith

#ifndef LIB_HTTP_CLIENT_HPP_
#define LIB_HTTP_CLIENT_HPP_

#include "http_header.hpp"

#include <chrono>
#include <string>
#include <system_error>
#include <tuple>
#include <variant>

namespace transferase {

[[nodiscard]] auto
download_http(const std::string &host,
              const std::string &port,
              const std::string &target,
              const std::string &outfile,
              const std::chrono::microseconds timeout)
  -> std::tuple<http_header, std::error_code>;

[[nodiscard]] auto
download_header_http(const std::string &host,
                     const std::string &port,
                     const std::string &target,
                     const std::chrono::microseconds timeout) -> http_header;

}  // namespace transferase

#endif  // LIB_HTTP_CLIENT_HPP_
