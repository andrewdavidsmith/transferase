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

#include "level_container.hpp"

#include <array>
#include <cstddef>  // for std::byte
#include <cstdint>
#include <iterator>  // for std::size
#include <string>
#include <system_error>
#include <utility>  // for std::move
#include <vector>

namespace transferase {

static constexpr std::uint32_t response_header_buffer_size = 256;
typedef std::array<char, response_header_buffer_size> response_header_buffer;

struct response_header {
  std::error_code status{};
  std::uint32_t response_size{};
  // ADS: doing the strange stuff below for cpplint...
  // clang-format off
  [[nodiscard]] auto error() const -> bool { return (status) ? true : false; }
  [[nodiscard]] auto summary() const -> std::string;
  // clang-format on
};

[[nodiscard]] auto
compose(char *first, const char *last,
        const response_header &hdr) -> std::error_code;

[[nodiscard]] auto
parse(const char *first, const char *last,
      response_header &hdr) -> std::error_code;

[[nodiscard]] auto
compose(response_header_buffer &buf,
        const response_header &hdr) -> std::error_code;

[[nodiscard]] auto
parse(const response_header_buffer &buf,
      response_header &hdr) -> std::error_code;

struct response_payload {
  std::vector<std::byte> payload;

  response_payload() = default;
  // ADS: update this to prevent a copy
  explicit response_payload(std::vector<std::byte> &&payload) noexcept :
    payload{std::move(payload)} {}

  // prevent copying and allow moving
  // clang-format off
  response_payload(const response_payload &) = delete;
  response_payload &operator=(const response_payload &) = delete;
  response_payload(response_payload &&) noexcept = default;
  response_payload &operator=(response_payload &&) noexcept = default;
  // clang-format on

  [[nodiscard]] auto
  n_bytes() const -> std::uint32_t {
    return std::size(payload);
  }
};

template <typename level_element> struct response {
  level_container<level_element> levels;
  [[nodiscard]] auto
  get_levels_n_bytes() const -> std::uint32_t {
    return sizeof(level_element) * size(levels);
  }
};

}  // namespace transferase

#endif  // SRC_RESPONSE_HPP_
