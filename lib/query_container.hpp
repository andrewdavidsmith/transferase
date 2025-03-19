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

#ifndef LIB_QUERY_CONTAINER_HPP_
#define LIB_QUERY_CONTAINER_HPP_

#include "query_element.hpp"

#include <cstddef>   // for std::size_t
#include <cstdint>   // for std::uint32_t
#include <iterator>  // for std::size, std::begin
#include <ranges>
#include <utility>  // for std::move
#include <vector>

namespace transferase {

struct query_container {
  typedef std::vector<query_element>::size_type size_type;

  /// @brief v the underlying container of query_element objects
  std::vector<query_element> v;

  query_container() = default;
  explicit query_container(const std::uint64_t data_size) : v(data_size) {}
  explicit query_container(std::vector<query_element> &&elements) :
    v{std::move(elements)} {}

  // prevent copying and allow moving
  // clang-format off
  query_container(const query_container &) = delete;
  query_container &operator=(const query_container &) = delete;
  query_container(query_container &&) noexcept = default;
  query_container &operator=(query_container &&) noexcept = default;
  // clang-format on

  /// @brief Resize the container.
  auto
  resize(const auto new_size) {
    v.resize(new_size);
  }

  /// @brief Reserve space in the container.
  auto
  reserve(const auto new_capacity) {
    v.reserve(new_capacity);
  }

  /// @brief Get the number of bytes used by this container.
  [[nodiscard]] auto
  get_n_bytes() const -> std::size_t {
    return sizeof(query_element) * std::size(v);
  }

  /// @brief Get a pointer to the underlying memory used by this container.
  [[nodiscard]] auto
  data(std::uint32_t n_bytes = 0) -> char * {
    return reinterpret_cast<char *>(v.data()) + n_bytes;  // NOLINT
  }

  /// @brief Get a const pointer to the underlying memory used by this
  /// container.
  [[nodiscard]] auto
  data(std::uint32_t n_bytes = 0) const -> const char * {
    return reinterpret_cast<const char *>(v.data()) + n_bytes;  // NOLINT
  }

  // clang-format off
  [[nodiscard]] auto begin() {return std::begin(v);}
  [[nodiscard]] auto begin() const {return std::cbegin(v);}
  [[nodiscard]] auto end() {return std::end(v);}
  [[nodiscard]] auto end() const {return std::cend(v);}
  [[nodiscard]] auto operator[](size_type pos) -> query_element& {return v[pos];}
  [[nodiscard]] auto operator[](size_type pos) const -> const query_element& {return v[pos];}
  // clang-format on

  [[nodiscard]] auto
  size() const noexcept -> std::size_t {
    return std::size(v);
  }

  [[nodiscard]] auto
  get_n_cpgs() const noexcept -> std::vector<std::uint32_t> {
    const auto count_cpgs = [](const auto x) { return x.stop - x.start; };
    return std::ranges::transform_view(v, count_cpgs) |
           std::ranges::to<std::vector>();
  }

  auto
  operator<=>(const query_container &other) const = default;
};

}  // namespace transferase

#endif  // LIB_QUERY_CONTAINER_HPP_
