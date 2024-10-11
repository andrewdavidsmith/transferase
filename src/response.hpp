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

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <system_error>

struct response {
  static constexpr std::uint32_t buf_size = 8;  // ADS: possibly not needed

  // buffers do not own underlying memory; keep the data alive!
  std::array<char, buf_size> buf{};  // ADS: possibly not needed
  std::error_condition status{make_error_condition(server_response_code::ok)};
  std::uint32_t methylome_size{};
  std::vector<counts_res> counts;  // counts_res from methylome.hpp

  auto summary() const -> std::string;
  auto summary_serial() const -> std::string;

  auto error() const -> bool {
    return bool(status);
  }

  auto from_buffer() -> std::error_condition;
  auto to_buffer() -> std::error_condition;

  auto get_counts_n_bytes() const -> std::uint32_t {
    return sizeof(counts_res) * size(counts);
  }

  static response not_found();
  static response bad_request();
};

#endif  // SRC_RESPONSE_HPP_
