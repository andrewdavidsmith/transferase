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

#ifndef SRC_QUERY_HPP_
#define SRC_QUERY_HPP_

#include <cstddef>  // for std::size_t
#include <cstdint>  // for std::uint32_t
#include <utility>  // for std::pair
#include <vector>

namespace xfrase {

typedef std::uint32_t q_elem_t;
// struct query_element {
//   q_elem_t start{};
//   q_elem_t stop{};
// };
typedef std::pair<q_elem_t, q_elem_t> query_element;

struct query {
  std::vector<query_element> v;
  typedef std::vector<query_element>::size_type size_type;
  query() = default;
  explicit query(const std::uint64_t n) : v(n) {}
  // ADS: below, for pass-through to v
  explicit query(std::vector<query_element> &&v) : v{v} {}
  auto
  resize(const auto x) {
    v.resize(x);
  }
  auto
  reserve(const auto x) {
    v.reserve(x);
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
  operator<=>(const query &other) const = default;
};

[[nodiscard]] inline auto
size(const query &qry) {
  return std::size(qry.v);
}

}  // namespace xfrase

#endif  // SRC_QUERY_HPP_
