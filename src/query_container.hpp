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

#ifndef SRC_QUERY_CONTAINER_HPP_
#define SRC_QUERY_CONTAINER_HPP_

#include "query_element.hpp"

#include <cstddef>   // for std::size_t
#include <cstdint>   // for std::uint32_t
#include <iterator>  // for std::size, std::begin
#include <utility>   // for std::move
#include <vector>

namespace transferase {

struct query_container {
  std::vector<query_element> v;
  typedef std::vector<query_element>::size_type size_type;

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

  auto
  resize(const auto new_size) {
    v.resize(new_size);
  }
  auto
  reserve(const auto new_capacity) {
    v.reserve(new_capacity);
  }
  [[nodiscard]] auto
  get_n_bytes() const -> std::size_t {
    return sizeof(query_element) * std::size(v);
  }
  [[nodiscard]] auto
  data() -> char * {
    return reinterpret_cast<char *>(v.data());
  }
  // clang-format off
  [[nodiscard]] auto begin() {return std::begin(v);}
  [[nodiscard]] auto begin() const {return std::cbegin(v);}
  [[nodiscard]] auto end() {return std::end(v);}
  [[nodiscard]] auto end() const {return std::cend(v);}
  [[nodiscard]] auto operator[](size_type pos) -> query_element& {return v[pos];}
  [[nodiscard]] auto operator[](size_type pos) const -> const query_element& {return v[pos];}
  // clang-format on

  auto
  operator<=>(const query_container &other) const = default;
};

[[nodiscard]] inline auto
size(const query_container &query) {
  return std::size(query.v);
}

}  // namespace transferase

#endif  // SRC_QUERY_CONTAINER_HPP_
