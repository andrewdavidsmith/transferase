/* MIT License
 *
 * Copyright (c) 2024-2026 Andrew D Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LIB_LEVEL_CONTAINER_FLAT_HPP_
#define LIB_LEVEL_CONTAINER_FLAT_HPP_

#include <algorithm>
#include <concepts>          // for std::integral
#include <cstddef>           // for std::size_t
#include <cstdint>           // for std::uint32_t
#include <initializer_list>  // for std::begin, std::end
#include <iterator>          // for std::size, std::cbegin, std::cend
#include <ranges>
#include <utility>  // for std::move
#include <vector>

namespace transferase {

template <typename level_element_type> struct level_container_flat {
  std::vector<level_element_type> v;
  typedef level_element_type value_type;
  typedef std::vector<level_element_type>::size_type size_type;

  // ADS: need to benchmark use of level set directly vs. starting with vector
  // of level_element

  // clang-format off
  level_container_flat() = default;
  explicit level_container_flat(const std::integral auto sz) noexcept : v(sz) {}
  explicit level_container_flat(std::vector<level_element_type> &&v) noexcept : v(std::move(v)) {}

  // prevent copying and allow moving
  level_container_flat(const level_container_flat &) = delete;
  auto operator=(const level_container_flat &) -> level_container_flat & = delete;
  level_container_flat(level_container_flat &&) noexcept = default;
  auto operator=(level_container_flat &&) noexcept -> level_container_flat & = default;

  [[nodiscard]] auto begin() {return std::begin(v);}
  [[nodiscard]] auto begin() const {return std::cbegin(v);}
  [[nodiscard]] auto end() {return std::end(v);}
  [[nodiscard]] auto end() const {return std::cend(v);}
  [[nodiscard]] auto operator[](size_type pos) -> level_element_type& {return v[pos];}
  [[nodiscard]] auto operator[](size_type pos) const -> const level_element_type& {return v[pos];}
  // clang-format on

  [[nodiscard]] auto
  get_wmeans(const std::uint32_t min_reads) const -> std::vector<double> {
    std::vector<double> u(std::size(v));
    std::ranges::transform(v, std::begin(u), [&](const auto &x) -> double {
      return x.n_reads() >= min_reads ? x.get_wmean() : -1.0;
    });
    return u;
  }

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
    return reinterpret_cast<const char *>(std::data(v));
  }

  [[nodiscard]] auto
  data() -> char * {
    return reinterpret_cast<char *>(std::data(v));
  }

  [[nodiscard]] auto
  size() const noexcept -> std::size_t {
    return std::size(v);
  }
};

}  // namespace transferase

#endif  // LIB_LEVEL_CONTAINER_FLAT_HPP_
