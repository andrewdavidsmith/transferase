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

#ifndef SRC_METHYLOME_DATA_HPP_
#define SRC_METHYLOME_DATA_HPP_

#include "cpg_index.hpp"
#if not defined(__APPLE__) && not defined(__MACH__)
#include "aligned_allocator.hpp"
#endif

#include <algorithm>
#include <cmath>    // for std::round
#include <cstddef>  // for std::size_t
#include <cstdint>  // for std::uint32_t
#include <format>
#include <iterator>  // for std::pair, std::size
#include <limits>    // for std::numeric_limits
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>  // for std::pair
#include <variant>  // IWYU pragma: keep
#include <vector>

struct counts_res;
struct counts_res_cov;
struct cpg_index_metadata;
struct methylome_metadata;

struct methylome_data {
  static constexpr std::string_view filename_extension{".m16"};

  typedef std::uint16_t m_count_t;
  typedef std::pair<m_count_t, m_count_t> m_elem;
#if not defined(__APPLE__) && not defined(__MACH__)
  typedef std::vector<m_elem, aligned_allocator<m_elem>> vec;
#else
  typedef std::vector<m_elem> vec;
#endif

  // ADS: use of n_cpgs to validate might be confusing and at least
  // need to be documented
  [[nodiscard]] static auto
  read(const std::string &filename, const methylome_metadata &meta)
    -> std::tuple<methylome_data, std::error_code>;

  [[nodiscard]] auto
  write(const std::string &filename,
        const bool zip = false) const -> std::error_code;

  [[nodiscard]] static auto
  get_n_cpgs_from_file(const std::string &filename) -> std::uint32_t;
  [[nodiscard]] static auto
  get_n_cpgs_from_file(const std::string &filename,
                       std::error_code &ec) -> std::uint32_t;

  [[nodiscard]] auto
  get_n_cpgs() const -> std::uint32_t;

  auto
  add(const methylome_data &rhs) -> methylome_data &;

  [[nodiscard]] auto
  hash() const -> std::uint64_t;

  typedef std::pair<std::uint32_t, std::uint32_t> offset_pair;

  [[nodiscard]] auto
  get_counts_cov(const cpg_index::vec &positions, const std::uint32_t offset,
                 const std::uint32_t start,
                 const std::uint32_t stop) const -> counts_res_cov;
  [[nodiscard]] auto
  get_counts(const cpg_index::vec &positions, const std::uint32_t offset,
             const std::uint32_t start,
             const std::uint32_t stop) const -> counts_res;

  // takes only the pair of positions within the methylome_data::vec
  // and accumulates between those
  [[nodiscard]] auto
  get_counts_cov(const std::uint32_t start,
                 const std::uint32_t stop) const -> counts_res_cov;
  [[nodiscard]] auto
  get_counts(const std::uint32_t start,
             const std::uint32_t stop) const -> counts_res;

  // takes a vector of pairs of positions (endpoints; eps) within the
  // methylome_data::vec and accumulates between each of those pairs of
  // enpoints
  [[nodiscard]] auto
  get_counts_cov(const std::vector<offset_pair> &eps) const
    -> std::vector<counts_res_cov>;
  [[nodiscard]] auto
  get_counts(const std::vector<offset_pair> &eps) const
    -> std::vector<counts_res>;

  [[nodiscard]] auto
  total_counts() const -> counts_res;
  [[nodiscard]] auto
  total_counts_cov() const -> counts_res_cov;

  // takes a bins size and a cpg_index and calculates the counts in
  // each bin along all chromosomes
  [[nodiscard]] auto
  get_bins(const std::uint32_t bin_size, const cpg_index &index,
           const cpg_index_metadata &meta) const -> std::vector<counts_res>;
  [[nodiscard]] auto
  get_bins_cov(const std::uint32_t bin_size, const cpg_index &index,
               const cpg_index_metadata &meta) const
    -> std::vector<counts_res_cov>;

  methylome_data::vec cpgs{};
  static constexpr auto record_size = sizeof(m_elem);
};

[[nodiscard]] inline auto
compose_methylome_data_filename(auto wo_extension) {
  wo_extension += methylome_data::filename_extension;
  return wo_extension;
}

[[nodiscard]] inline auto
size(const methylome_data &m) -> std::size_t {
  return std::size(m.cpgs);
}

[[nodiscard]] auto
read_methylome_data(const std::string &methylome_file)
  -> std::tuple<methylome_data, methylome_metadata, std::error_code>;

[[nodiscard]] auto
read_methylome_data(const std::string &methylome_file,
                    const std::string &methylome_meta_file)
  -> std::tuple<methylome_data, methylome_metadata, std::error_code>;

template <typename T, typename U>
inline auto
round_to_fit(U &a, U &b) -> void {
  // ADS: assign the max of a and b to be the max possible value; the
  // other one gets a fractional value then multiplied by max possible
  const U c = std::max(a, b);
  a = (a == c) ? std::numeric_limits<T>::max()
               : std::round((a / static_cast<double>(c)) *
                            std::numeric_limits<T>::max());
  b = (b == c) ? std::numeric_limits<T>::max()
               : std::round((b / static_cast<double>(c)) *
                            std::numeric_limits<T>::max());
}

template <typename T, typename U>
inline auto
conditional_round_to_fit(U &a, U &b) -> void {
  // ADS: optimization possible here?
  if (std::max(a, b) > std::numeric_limits<T>::max())
    round_to_fit<T>(a, b);
}

#endif  // SRC_METHYLOME_DATA_HPP_
