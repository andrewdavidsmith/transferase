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

#include "methylome.hpp"  // for counts_res

#include "mc16_error.hpp"
#include "utilities.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <system_error>

static constexpr std::uint32_t response_buf_size = 256;  // how much needed?
// buffers do not own underlying memory; keep the data alive!
typedef std::array<char, response_buf_size> response_buffer;

struct response_header {
  std::error_code status{make_error_code(server_response_code::ok)};
  std::uint32_t methylome_size{};

  auto error() const -> bool {
    return bool(status);
  }

  auto summary() const -> std::string;
  auto summary_serial() const -> std::string;

  static response_header not_found();
  static response_header bad_request();
};

auto
to_chars(char *first, char *last,
         const response_header &header) -> mc16_to_chars_result;

auto
from_chars(const char *first, const char *last,
           response_header &header) -> mc16_from_chars_result;

struct response {
  std::vector<counts_res> counts;  // counts_res from methylome.hpp

  auto get_counts_n_bytes() const -> std::uint32_t {
    return sizeof(counts_res) * size(counts);
  }
};

#endif  // SRC_RESPONSE_HPP_
