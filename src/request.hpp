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

#ifndef SRC_REQUEST_HPP_
#define SRC_REQUEST_HPP_

#include "mxe_error.hpp"
#include "utilities.hpp"

#include <array>
#include <cstdint>  // std::uint32_t
#include <string>
#include <system_error>
#include <utility>  // pair<>
#include <vector>

static constexpr std::uint32_t request_header_buf_size = 256;  // full request
typedef std::array<char, request_header_buf_size> request_header_buffer;

struct request_header {
  enum class request_type : std::uint32_t {
    counts = 0,
    counts_cov = 1,
    bin_counts = 2,
    bin_counts_cov = 3,
    n_request_types = 4,
  };
  std::string accession;
  std::uint32_t methylome_size{};
  request_type rq_type{};
  auto summary() const -> std::string;
  auto is_valid_type() const -> bool {
    return rq_type < request_type::n_request_types;
  }
};

template <>
struct std::formatter<request_header::request_type>
  : std::formatter<std::string> {
  auto format(const request_header::request_type &rt,
              std::format_context &ctx) const {
    return std::formatter<std::string>::format(
      std::format("{}", std::to_underlying(rt)), ctx);
  }
};

template <>
struct std::formatter<request_header> : std::formatter<std::string> {
  auto format(const request_header &rh, std::format_context &ctx) const {
    return std::formatter<std::string>::format(
      std::format("{}\t{}\t{}", rh.accession, rh.methylome_size, rh.rq_type),
      ctx);
  }
};

auto
to_chars(char *first, char *last,
         const request_header &header) -> mxe_to_chars_result;

auto
from_chars(const char *first, const char *last,
           request_header &header) -> mxe_from_chars_result;

auto
compose(request_header_buffer &buf,
        const request_header &header) -> compose_result;

auto
parse(const request_header_buffer &buf, request_header &header) -> parse_result;

struct request {
  typedef std::pair<std::uint32_t, std::uint32_t> offset_type;

  std::uint32_t n_intervals{};
  std::vector<offset_type> offsets;

  auto summary() const -> std::string;

  auto get_offsets_n_bytes() const -> uint32_t {
    return sizeof(offset_type) * size(offsets);
  }
  auto get_offsets_data() -> char * {
    return reinterpret_cast<char *>(offsets.data());
  }
};

auto
to_chars(char *first, char *last, const request &req) -> mxe_to_chars_result;

auto
from_chars(const char *first, const char *last,
           request &req) -> mxe_from_chars_result;

#endif  // SRC_REQUEST_HPP_
