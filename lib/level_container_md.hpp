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

#ifndef LIB_LEVEL_CONTAINER_MD_HPP_
#define LIB_LEVEL_CONTAINER_MD_HPP_

#include <concepts>  // for std::integral
#include <cstddef>   // for std::size_t
#include <cstdint>
#include <initializer_list>  // for std::begin, std::end
#include <iterator>          // for std::size, std::cbegin, std::cend
#include <utility>           // for std::move
#include <vector>

namespace transferase {

template <typename level_element_type> struct level_container_md {
  std::uint32_t n_rows{};
  std::uint32_t n_cols{};
  std::vector<level_element_type> v;
  typedef level_element_type value_type;
  typedef std::vector<level_element_type>::size_type size_type;

  // ADS: need to benchmark use of level set directly vs. starting with vector
  // of level_element

  // clang-format off
  level_container_md() = default;
  explicit level_container_md(const std::integral auto n_rows,
                              const std::integral auto n_cols) noexcept :
    n_rows(n_rows), n_cols(n_cols), v(n_rows*n_cols) {}
  explicit level_container_md(std::vector<level_element_type> &&v) noexcept :
    n_rows(std::size(v)), n_cols(1), v(std::move(v)) {}

  // prevent copying and allow moving
  level_container_md(const level_container_md &) = delete;
  auto operator=(const level_container_md &) -> level_container_md & = delete;
  level_container_md(level_container_md &&) noexcept = default;
  auto operator=(level_container_md &&) noexcept -> level_container_md & = default;

  auto
  operator[](const std::integral auto i,
             const std::integral auto j) -> level_element_type {
    return v[i*n_cols + j];
  }

  [[nodiscard]] auto begin() {return std::begin(v);}
  [[nodiscard]] auto begin() const {return std::cbegin(v);}
  [[nodiscard]] auto end() {return std::end(v);}
  [[nodiscard]] auto end() const {return std::cend(v);}
  [[nodiscard]] auto operator[](size_type pos) -> level_element_type& {return v[pos];}
  [[nodiscard]] auto operator[](size_type pos) const -> const level_element_type& {return v[pos];}
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
    return sizeof(level_element_type) * std::size(v);
  }

  [[nodiscard]] auto
  data() const -> const char * {
    return reinterpret_cast<const char *>(v.data());
  }

  [[nodiscard]] auto
  data() -> char * {
    return reinterpret_cast<char *>(v.data());
  }

  [[nodiscard]] auto
  size() const noexcept -> std::size_t {
    return std::size(v);
  }
};

}  // namespace transferase

#endif  // LIB_LEVEL_CONTAINER_MD_HPP_
