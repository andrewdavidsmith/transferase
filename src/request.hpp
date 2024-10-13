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

#include "mc16_error.hpp"
#include "utilities.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <system_error>
#include <utility>  // pair<>
#include <vector>

static constexpr std::uint32_t request_buf_size = 256;  // full request
typedef std::array<char, request_buf_size> request_buffer;

struct request_header {
  std::string accession;
  std::uint32_t methylome_size{};
  std::uint32_t request_type{};
  auto summary() const -> std::string;
  auto summary_serial() const -> std::string;
};

auto
to_chars(char *first, char *last,
         const request_header &header) -> mc16_to_chars_result;

auto
from_chars(const char *first, const char *last,
           request_header &header) -> mc16_from_chars_result;

auto
compose(request_buffer &buf, const request_header &header) -> compose_result;

auto
parse(const request_buffer &buf, request_header &header) -> parse_result;

struct request {
  typedef std::pair<std::uint32_t, std::uint32_t> offset_type;

  std::uint32_t n_intervals{};
  std::vector<offset_type> offsets;

  auto summary() const -> std::string;
  auto summary_serial() const -> std::string;

  auto get_offsets_n_bytes() const -> uint32_t {
    return sizeof(offset_type) * size(offsets);
  }
  auto get_offsets_data() -> char * {
    return reinterpret_cast<char *>(offsets.data());
  }
};

auto
to_chars(char *first, char *last,
         const request &req) -> mc16_to_chars_result;

auto
from_chars(const char *first, const char *last,
           request &req) -> mc16_from_chars_result;

#endif  // SRC_REQUEST_HPP_
