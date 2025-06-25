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

#ifndef LIB_METHYLOME_DATA_HPP_
#define LIB_METHYLOME_DATA_HPP_

#include "level_container.hpp"

#include <algorithm>
#include <cmath>     // for std::round
#include <concepts>  // for std::integral
#include <cstddef>   // for std::size_t
#include <cstdint>   // for std::uint32_t
#include <filesystem>
#include <format>
#include <iterator>  // for std::size
#include <limits>    // for std::numeric_limits
#include <string>
#include <system_error>
#include <utility>  // std::move
#include <variant>  // for std::hash
#include <vector>

namespace transferase {

struct genome_index;
struct methylome_metadata;
struct query_container;

typedef std::uint16_t mcount_t;

/// @brief A pair of counts for methylated and unmethylated observations at a
/// single site (e.g., CpG) in the genome.
struct mcount_pair {
  /// Number of methylated observations.
  mcount_t n_meth{};

  /// Number of unmethylated observations.
  mcount_t n_unmeth{};

  mcount_pair() = default;

  mcount_pair(const std::integral auto n_meth,
              const std::integral auto n_unmeth) :
    n_meth{static_cast<mcount_t>(n_meth)},
    n_unmeth{static_cast<mcount_t>(n_unmeth)} {}
  // ADS: need spaceship here because of constness
  [[nodiscard]] auto
  operator<=>(const mcount_pair &) const = default;
};

struct methylome_data {
  static constexpr auto filename_extension{".m16"};

  typedef std::vector<mcount_pair> vec;

  methylome_data() = default;
  explicit methylome_data(methylome_data::vec &&cpgs) : cpgs{std::move(cpgs)} {}

  // prevent copy, allow move
  // clang-format off
  methylome_data(const methylome_data &) = delete;
  auto operator=(const methylome_data &) -> methylome_data & = delete;
  methylome_data(methylome_data &&) noexcept = default;
  auto operator=(methylome_data &&) noexcept -> methylome_data & = default;
  // clang-format on

  [[nodiscard]] auto
  tostring() const noexcept -> std::string {
    return std::format(R"json({{"size": {}}})json", get_n_cpgs());
  }

  [[nodiscard]] static auto
  read(const std::string &dirname, const std::string &methylome_name,
       const methylome_metadata &meta,
       std::error_code &ec) noexcept -> methylome_data;

#ifndef TRANSFERASE_NOEXCEPT
  [[nodiscard]] static auto
  read(const std::string &dirname, const std::string &methylome_name,
       const methylome_metadata &meta) -> methylome_data {
    std::error_code ec;
    auto data = read(dirname, methylome_name, meta, ec);
    if (ec)
      throw std::system_error(ec);
    return data;
  }
#endif

  [[nodiscard]] auto
  write(const std::string &filename,
        const bool zip = false) const noexcept -> std::error_code;

  [[nodiscard]] static auto
  get_n_cpgs_from_file(const std::string &filename) noexcept -> std::uint32_t;

  [[nodiscard]] static auto
  get_n_cpgs_from_file(const std::string &filename,
                       std::error_code &ec) noexcept -> std::uint32_t;

  [[nodiscard]] auto
  get_n_cpgs() const noexcept -> std::uint32_t;

  auto
  add(const methylome_data &rhs) noexcept -> void;

  [[nodiscard]] auto
  hash() const noexcept -> std::uint64_t;

  /// @brief Get methylation levels for each of a set of query intervals.
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const transferase::query_container &query) const noexcept
    -> level_container<lvl_elem_t>;

  /// @brief Get methylation levels for each of a set of query intervals.
  template <typename lvl_elem_t>
  auto
  get_levels(const transferase::query_container &query,
             level_container<lvl_elem_t>::iterator &res) const noexcept -> void;

  /// Get global methylation levels.
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  global_levels() const noexcept -> lvl_elem_t;

  /// @brief Get methylation levels for each fixed size bin in the genome.
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const std::uint32_t bin_size, const genome_index &index)
    const noexcept -> level_container<lvl_elem_t>;

  /// @brief Get methylation levels for each fixed size bin in the genome.
  template <typename lvl_elem_t>
  auto
  get_levels(const std::uint32_t bin_size, const genome_index &index,
             level_container<lvl_elem_t>::iterator &res) const noexcept -> void;

  /// @brief Get methylation levels for each fixed size sliding window in the
  /// genome.
  template <typename lvl_elem_t>
  [[nodiscard]] auto
  get_levels(const std::uint32_t window_size, const std::uint32_t window_step,
             const genome_index &index) const noexcept
    -> level_container<lvl_elem_t>;

  /// @brief Get methylation levels for each fixed size sliding window in the
  /// genome.
  template <typename lvl_elem_t>
  auto
  get_levels(const std::uint32_t window_size, const std::uint32_t window_step,
             const genome_index &index,
             level_container<lvl_elem_t>::iterator d_first) const noexcept
    -> void;

  [[nodiscard]] static auto
  compose_filename(auto wo_extension) {
    wo_extension += filename_extension;
    return wo_extension;
  }

  [[nodiscard]] static auto
  compose_filename(const auto &directory, const auto &name) {
    const auto wo_extn = (std::filesystem::path{directory} / name).string();
    return std::format("{}{}", wo_extn, filename_extension);
  }

  methylome_data::vec cpgs{};
  static constexpr auto record_size = sizeof(mcount_pair);
};

/// Given two integer count values, round the values so that they keep their
/// ratio but fit into a smaller specified type.
template <typename T, typename U>
inline auto
round_to_fit(U &a, U &b) noexcept -> void {
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

/// Given two integer count values, if those values do not fit in a
/// specified type, round them but keep their ratio and shrink them to
/// fit into the smaller specified type.
template <typename T, typename U>
inline auto
conditional_round_to_fit(U &a, U &b) noexcept -> void {
  // ADS: optimization possible here?
  if (std::max(a, b) > std::numeric_limits<T>::max())
    round_to_fit<T>(a, b);
}

[[nodiscard]] inline auto
size(const methylome_data &data) {
  return std::size(data.cpgs);
}

}  // namespace transferase

/// Specialization of std::hash for methylome_data
template <> struct std::hash<transferase::methylome_data> {
  auto
  operator()(const transferase::methylome_data &data) const noexcept
    -> std::size_t {
    return data.hash();
  }
};

#endif  // LIB_METHYLOME_DATA_HPP_
