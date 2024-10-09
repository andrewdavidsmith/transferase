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

#ifndef SRC_METHYLOME_HPP_
#define SRC_METHYLOME_HPP_

#include "aligned_allocator.hpp"
#include "cpg_index.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <cstddef>

struct counts_res {
  std::uint32_t n_meth{};
  std::uint32_t n_unmeth{};
  std::uint32_t n_covered{};
};

template <>
struct std::formatter<counts_res> : std::formatter<std::string> {
  auto
  format(const counts_res &cr, std::format_context &ctx) const {
    return std::formatter<std::string>::format(
      std::format(R"({{"n_meth": {}, "n_unmeth": {}, "n_covered": {}}})",
                  cr.n_meth, cr.n_unmeth, cr.n_covered),
      ctx);
  }
};

// ADS TODO: error codes
struct methylome {
  typedef std::uint16_t m_count_t;
  typedef std::pair<m_count_t, m_count_t> m_elem;
  typedef std::vector<m_elem, aligned_allocator<m_elem>> vec;

  [[nodiscard]] auto
  read(const std::string &filename, const uint32_t n_cpgs = 0) -> int;

  [[nodiscard]] auto
  write(const std::string &filename, const bool zip = false) const -> int;

  auto
  operator+=(const methylome &rhs) -> methylome&;

  [[nodiscard]] auto
  get_counts(const cpg_index::vec &positions, const std::uint32_t offset,
             const std::uint32_t start, const std::uint32_t stop) const
    -> counts_res;

  [[nodiscard]] auto
  total_counts() const -> counts_res;

  // takes only the pair of positions within the methylome::vec
  // and accumulates between those
  [[nodiscard]] auto
  get_counts(const std::uint32_t start, const std::uint32_t stop) const
    -> counts_res;

  // takes a vector of pairs of positions (endpoints; eps) within the
  // methylome::vec and accumulates between each of those pairs of
  // enpoints
  typedef std::pair<std::uint32_t, std::uint32_t> offset_pair;
  [[nodiscard]] auto
  get_counts(const std::vector<offset_pair> &eps) const
    -> std::vector<counts_res>;

  methylome::vec cpgs{};
  static constexpr auto record_size = sizeof(m_elem);
};

[[nodiscard]] inline auto
size(const methylome &m) -> std::size_t { return std::size(m.cpgs); }

#endif  // SRC_METHYLOME_HPP_
