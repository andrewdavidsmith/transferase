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

#ifndef SRC_RESPONSE_HPP_
#define SRC_RESPONSE_HPP_

#include "methylome.hpp"  // for counts_res_cov

#include "utilities.hpp"
#include "xfrase_error.hpp"

#include <array>
#include <cstddef>  // for std::byte
#include <cstdint>
#include <iterator>  // for std::size
#include <string>
#include <system_error>
#include <vector>

static constexpr std::uint32_t response_buf_size = 256;  // how much needed?
// buffers do not own underlying memory; keep the data alive!
typedef std::array<char, response_buf_size> response_header_buffer;

struct response_header {
  std::error_code status{make_error_code(server_response_code::ok)};
  std::uint32_t response_size{};

  // ADS: doing the strange stuff below for cpplint...
  [[nodiscard]] auto
  error() const -> bool {
    return (status) ? true : false;
  }

  [[nodiscard]] auto
  summary() const -> std::string;
};

[[nodiscard]] auto
compose(char *first, char *last,
        const response_header &header) -> compose_result;

[[nodiscard]] auto
parse(const char *first, const char *last,
      response_header &header) -> parse_result;

[[nodiscard]] auto
compose(std::array<char, response_buf_size> &buf,
        const response_header &hdr) -> compose_result;

[[nodiscard]] auto
parse(const std::array<char, response_buf_size> &buf,
      response_header &hdr) -> parse_result;

struct response_payload {
  std::vector<std::byte> payload;
  [[nodiscard]] auto
  n_bytes() const -> std::uint32_t {
    return std::size(payload);
  }
};

template <typename counts_type> struct response {
  std::vector<counts_type> counts;  // counts_type is from methylome.hpp
  [[nodiscard]] auto
  get_counts_n_bytes() const -> std::uint32_t {
    return sizeof(counts_type) * size(counts);
  }
};

#endif  // SRC_RESPONSE_HPP_
