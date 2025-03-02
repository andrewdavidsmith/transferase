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

#include "level_element.hpp"

#include <cassert>
#include <concepts>  // for std::integral
#include <cstddef>   // for std::size_t
#include <cstdint>
#include <initializer_list>  // for std::begin, std::end
#include <iostream>
#include <iterator>  // for std::size, std::cbegin, std::cend
#include <ranges>
#include <utility>  // for std::move
#include <vector>

namespace transferase {

template <typename level_element_type> struct level_container_md {
  std::uint32_t n_rows{};
  std::uint32_t n_cols{};
  std::vector<level_element_type> v;
  typedef level_element_type value_type;
  typedef std::vector<level_element_type>::size_type size_type;
  typedef std::vector<level_element_type>::iterator iterator;
  typedef std::vector<level_element_type>::const_iterator const_iterator;

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
  level_container_md(level_container_md &&) = default;
  auto operator=(level_container_md &&) -> level_container_md & = default;

  [[nodiscard]] auto
  operator[](const std::integral auto i,
             const std::integral auto j) const noexcept -> level_element_type {
    return v[j * n_rows + i];
  }

  auto
  operator[](const std::integral auto i,
             const std::integral auto j) noexcept -> level_element_type & {
    return v[j * n_rows + i];
  }

  [[nodiscard]] auto begin() noexcept {return std::begin(v);}
  [[nodiscard]] auto begin() const noexcept {return std::cbegin(v);}
  [[nodiscard]] auto end() noexcept {return std::end(v);}
  [[nodiscard]] auto end() const noexcept {return std::cend(v);}
  [[nodiscard]] auto operator[](size_type pos) noexcept
    -> level_element_type& {return v[pos];}
  [[nodiscard]] auto operator[](size_type pos) const noexcept
    -> const level_element_type& {return v[pos];}
  // clang-format on

  [[nodiscard]] auto
  get_wmeans(const std::uint32_t min_reads) const
    -> std::vector<std::vector<float>> {
    std::vector<std::vector<float>> u(n_cols, std::vector<float>(n_rows));
    for (const auto col_id : std::views::iota(0u, n_cols))
      std::ranges::transform(
        column_itr(col_id), column_itr(col_id) + n_rows, std::begin(u[col_id]),
        [min_reads](const auto &val) -> double {
          return val.n_reads() >= min_reads ? val.get_wmean() : -1.0;
        });
    return u;
  }

  auto
  resize(const std::integral auto new_size) noexcept {
    v.resize(new_size);
  }

  auto
  resize(const std::integral auto n_rows_arg,
         const std::integral auto n_cols_arg) noexcept {
    v.resize(n_rows_arg * n_cols_arg);
    n_rows = n_rows_arg;
    n_cols = n_cols_arg;
  }

  auto
  reserve(const std::integral auto new_capacity) noexcept {
    v.reserve(new_capacity);
  }

  [[nodiscard]] auto
  get_n_bytes() const noexcept -> std::size_t {
    return sizeof(level_element_type) * std::size(v);
  }

  [[nodiscard]] auto
  data() const noexcept -> const char * {
    return reinterpret_cast<const char *>(v.data());
  }

  [[nodiscard]] auto
  data_at_column(const std::integral auto col_id) const noexcept -> const
    char * {
    return reinterpret_cast<const char *>(&v[col_id * n_rows]);
  }

  [[nodiscard]] auto
  column_itr(const std::integral auto col_id) const noexcept -> const_iterator {
    return std::cbegin(v) + n_rows * col_id;
  }

  [[nodiscard]] auto
  column_itr(const std::integral auto col_id) noexcept -> iterator {
    return std::begin(v) + n_rows * col_id;
  }

  [[nodiscard]] auto
  data() noexcept -> char * {
    return reinterpret_cast<char *>(v.data());
  }

  [[nodiscard]] auto
  data_at_column(const std::integral auto col_id) noexcept -> char * {
    return reinterpret_cast<char *>(&v[col_id * n_rows]);
  }

  [[nodiscard]] auto
  size() const noexcept -> std::size_t {
    return std::size(v);
  }

  [[nodiscard]] auto
  tostring() const noexcept -> std::string {
    std::string s;
    for (const auto i : std::views::iota(0u, n_rows)) {
      s += operator[](i, 0).tostring_counts();
      for (const auto j : std::views::iota(1u, n_cols)) {
        s += '\t';
        s += operator[](i, j).tostring_counts();
      }
      s += '\n';
    }
    return s;
  }

  /// Add a column by growing the level_container_md underlying memory and
  /// copying the new column values into place.
  auto
  add_column(const auto &c) noexcept {
    assert(n_rows == 0 || std::size(c) == n_rows);
    n_rows = (n_rows == 0) ? std::size(c) : n_rows;
    std::ranges::copy_n(std::cbegin(c), n_rows, std::back_inserter(v));
    ++n_cols;
  }
};

[[nodiscard]] auto
read_level_container_md(const std::string &filename,
                        std::error_code &error) noexcept
  -> level_container_md<level_element_t>;

[[nodiscard]] auto
read_level_container_md_covered(const std::string &filename,
                                std::error_code &error) noexcept
  -> level_container_md<level_element_covered_t>;

}  // namespace transferase

#endif  // LIB_LEVEL_CONTAINER_MD_HPP_
