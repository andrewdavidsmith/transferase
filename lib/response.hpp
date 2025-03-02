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

#ifndef LIB_RESPONSE_HPP_
#define LIB_RESPONSE_HPP_

#include "level_container_md.hpp"

#include <array>
#include <cstddef>  // for std::byte
#include <cstdint>
#include <cstring>
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
  std::uint32_t cols{};
  std::uint32_t rows{};
  [[nodiscard]] auto
  error() const noexcept -> bool {
    return (status) ? true : false;
  }
  [[nodiscard]] auto
  summary() const noexcept -> std::string;
};

[[nodiscard]] auto
compose(char *first, const char *last,
        const response_header &hdr) noexcept -> std::error_code;

[[nodiscard]] auto
parse(const char *first, const char *last,
      response_header &hdr) noexcept -> std::error_code;

[[nodiscard]] auto
compose(response_header_buffer &buf,
        const response_header &hdr) noexcept -> std::error_code;

[[nodiscard]] auto
parse(const response_header_buffer &buf,
      response_header &hdr) noexcept -> std::error_code;

struct response_payload {
  typedef std::byte value_type;
  std::vector<value_type> payload;

  response_payload() = default;
  explicit response_payload(const std::size_t n_bytes) noexcept :
    payload{n_bytes} {}
  explicit response_payload(std::vector<std::byte> &&payload) noexcept :
    payload{std::move(payload)} {}

  // prevent copying and allow moving
  // clang-format off
  response_payload(const response_payload &) = delete;
  auto operator=(const response_payload &) -> response_payload & = delete;
  response_payload(response_payload &&) noexcept = default;
  auto operator=(response_payload &&) -> response_payload & = default;
  // clang-format on

  template <typename lvl_elem>
  [[nodiscard]] static auto
  from_levels(const level_container_md<lvl_elem> &levels,
              std::error_code &error) -> response_payload {
    // ADS: slower than needed; copy happening here is not needed
    error = std::error_code{};  // clear this in case it was set
    const auto tot_bytes = std::size(levels) * sizeof(lvl_elem);
    response_payload rp(tot_bytes);
    std::memcpy(rp.data(), levels.data(), levels.get_n_bytes());
    return rp;
  }

  [[nodiscard]] auto
  data() noexcept -> char * {
    // NOLINTNEXTLINE(*-reinterpret-cast)
    return reinterpret_cast<char *>(payload.data());
  }

  [[nodiscard]] auto
  data() const noexcept -> const char * {
    // NOLINTNEXTLINE(*-reinterpret-cast)
    return reinterpret_cast<const char *>(payload.data());
  }

  [[nodiscard]] auto
  data_at(const std::size_t byte_offset,
          std::error_code &error) noexcept -> char * {
    if (byte_offset >= std::size(payload) * sizeof(value_type)) {
      error = std::make_error_code(std::errc::result_out_of_range);
      return nullptr;
    }
    error = std::error_code{};
    // NOLINTNEXTLINE(*-reinterpret-cast)
    return reinterpret_cast<char *>(payload.data()) + byte_offset;
  }

  [[nodiscard]] auto
  data_at(const std::size_t byte_offset,
          std::error_code &error) const noexcept -> const char * {
    if (byte_offset >= std::size(payload) * sizeof(value_type)) {
      error = std::make_error_code(std::errc::result_out_of_range);
      return nullptr;
    }
    error = std::error_code{};
    // NOLINTNEXTLINE(*-reinterpret-cast)
    return reinterpret_cast<const char *>(payload.data()) + byte_offset;
  }

  [[nodiscard]] auto
  get_n_bytes() const -> std::uint32_t {
    return std::size(payload);
  }
};

}  // namespace transferase

#endif  // LIB_RESPONSE_HPP_
