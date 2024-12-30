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

#include "cpg_index_data.hpp"  // for cpg_index_data::vec
#if not defined(__APPLE__) && not defined(__MACH__)
#include "aligned_allocator.hpp"
#endif

#include <algorithm>
#include <cmath>    // for std::round
#include <cstddef>  // for std::size_t
#include <cstdint>  // for std::uint32_t
#include <filesystem>
#include <format>
#include <iterator>  // for std::pair, std::size
#include <limits>    // for std::numeric_limits
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>  // for std::pair
#include <vector>

namespace xfrase {

struct cpg_index;
struct counts_res;
struct counts_res_cov;
struct methylome_metadata;
struct query;

typedef std::uint16_t m_count_t;
// struct m_count_elem {
//   m_count_t n_meth{};
//   m_count_t n_unmeth{};
// };
typedef std::pair<m_count_t, m_count_t> m_count_elem;
// ADS: allow for accumulation
typedef std::uint32_t m_count_acc_t;

struct methylome_data {
  static constexpr auto filename_extension{".m16"};

  typedef std::pair<m_count_t, m_count_t> m_elem;
#if not defined(__APPLE__) && not defined(__MACH__)
  typedef std::vector<m_elem, aligned_allocator<m_elem>> vec;
#else
  typedef std::vector<m_elem> vec;
#endif

  [[nodiscard]] static auto
  read(const std::string &filename, const methylome_metadata &meta,
       std::error_code &ec) -> methylome_data;

  [[nodiscard]] static auto
  read(const std::string &dirname, const std::string &methylome_name,
       const methylome_metadata &meta, std::error_code &ec) -> methylome_data;

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

  [[nodiscard]] auto
  get_counts_cov(const cpg_index_data::vec &positions,
                 const std::uint32_t offset, const std::uint32_t start,
                 const std::uint32_t stop) const -> counts_res_cov;
  [[nodiscard]] auto
  get_counts(const cpg_index_data::vec &positions, const std::uint32_t offset,
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
  get_counts_cov(const xfrase::query &qry) const -> std::vector<counts_res_cov>;
  [[nodiscard]] auto
  get_counts(const xfrase::query &qry) const -> std::vector<counts_res>;

  [[nodiscard]] auto
  total_counts() const -> counts_res;
  [[nodiscard]] auto
  total_counts_cov() const -> counts_res_cov;

  // takes a bins size and a cpg_index and calculates the counts in
  // each bin along all chromosomes
  [[nodiscard]] auto
  get_bins(const std::uint32_t bin_size,
           const cpg_index &index) const -> std::vector<counts_res>;
  [[nodiscard]] auto
  get_bins_cov(const std::uint32_t bin_size,
               const cpg_index &index) const -> std::vector<counts_res_cov>;

  methylome_data::vec cpgs{};
  static constexpr auto record_size = sizeof(m_elem);
};

[[nodiscard]] inline auto
compose_methylome_data_filename(auto wo_extension) {
  wo_extension += methylome_data::filename_extension;
  return wo_extension;
}

[[nodiscard]] inline auto
compose_methylome_data_filename(const auto &directory, const auto &name) {
  const auto wo_extn = (std::filesystem::path{directory} / name).string();
  return std::format("{}{}", wo_extn, methylome_data::filename_extension);
}

[[nodiscard]] inline auto
size(const methylome_data &m) -> std::size_t {
  return std::size(m.cpgs);
}

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

}  // namespace xfrase

// methylome_data errors
enum class methylome_data_code : std::uint32_t {
  ok = 0,
  error_reading = 1,
  error_writing = 2,
  inconsistent = 3,
};

template <>
struct std::is_error_code_enum<methylome_data_code> : public std::true_type {};

struct methylome_data_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override {return "methylome_data";}
  auto message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "error reading methylome data"s;
    case 2: return "error writing methylome data"s;
    case 3: return "inconsistent methylome data"s;
    }
    std::unreachable();  // hopefully
  }
  // clang-format on
};

inline auto
make_error_code(methylome_data_code e) -> std::error_code {
  static auto category = methylome_data_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_METHYLOME_DATA_HPP_
