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

#ifndef LIB_LEVEL_ELEMENT_HPP_
#define LIB_LEVEL_ELEMENT_HPP_

#include <algorithm>  // for std::max
#include <cstdint>
#include <format>
#include <string>

namespace transferase {

/// @brief Pair of counts representing methylation level.
///
/// The counts correspond to number of methylated and unmethylated
/// observations for the purpose of representing methylation level
/// through a genomic interval.
struct level_element_t {
  /// Number of observations (e.g., sites in reads) corresponding to a
  /// methylated state.
  std::uint32_t n_meth{};

  /// Number of observations (e.g., sites in reads) corresponding to an
  /// unmethylated state.
  std::uint32_t n_unmeth{};

  /// Number of observations (e.g., sites in reads) contributing to either
  /// state.
  [[nodiscard]] constexpr auto
  n_reads() const noexcept -> std::uint32_t {
    return n_meth + n_unmeth;
  }

  /// Get the weighted mean methylation level: the number of methylated
  /// observations divided by the number of unmethylated observations.
  [[nodiscard]] constexpr auto
  get_wmean() const noexcept -> float {
    return static_cast<float>(n_meth) / std::max(1u, n_meth + n_unmeth);
  }

  /// Get a string representation for the counts held by this object.
  [[nodiscard]] constexpr auto
  tostring_counts() const noexcept -> std::string {
    return std::format("{}\t{}", n_meth, n_unmeth);
  }

  /// Get a string representation as used in dnmtools counts
  [[nodiscard]] constexpr auto
  tostring_classic() const noexcept -> std::string {
    return std::format("{:.6}\t{}", get_wmean(), n_reads());
  }

  auto
  operator<=>(const level_element_t &) const = default;

  static constexpr auto hdr_fmt = "{}_M{}{}_U";
};

/// @brief Triple of counts for methylation level with number of sites covered.
///
/// Includes three counts: number of methylated and unmethylated
/// observations, and number of sites contributing at least one
/// observation, for the purpose of representing methylation level
/// through a genomic interval.
struct level_element_covered_t {
  /// Number of observations (e.g., sites in reads) corresponding to a
  /// methylated state.
  std::uint32_t n_meth{};

  /// Number of observations (e.g., sites in reads) corresponding to an
  /// unmethylated state.
  std::uint32_t n_unmeth{};

  /// Number of sites in the corresponding genomic interval that contribute at
  /// least one observation to the n_meth or n_unmeth values.
  std::uint32_t n_covered{};

  /// Number of observations (e.g., sites in reads) contributing to either
  /// state.
  [[nodiscard]] constexpr auto
  n_reads() const noexcept -> std::uint32_t {
    return n_meth + n_unmeth;
  }

  /// Get the weighted mean methylation level: the number of methylated
  /// observations divided by the number of unmethylated observations.
  [[nodiscard]] constexpr auto
  get_wmean() const noexcept -> float {
    return static_cast<float>(n_meth) / std::max(1u, n_meth + n_unmeth);
  }

  /// Get a string representation for the counts held by this object.
  [[nodiscard]] constexpr auto
  tostring_counts() const noexcept -> std::string {
    return std::format("{}\t{}\t{}", n_meth, n_unmeth, n_covered);
  }

  /// Get a string representation as used in dnmtools counts
  [[nodiscard]] constexpr auto
  tostring_classic() const noexcept -> std::string {
    return std::format("{:.6}\t{}\t{}", get_wmean(), n_reads(), n_covered);
  }

  auto
  operator<=>(const level_element_covered_t &) const = default;

  static constexpr auto hdr_fmt = "{}_M{}{}_U{}{}_C";
};

}  // namespace transferase

template <>
struct std::formatter<transferase::level_element_t>
  : std::formatter<std::string> {
  auto
  format(const transferase::level_element_t &l,
         std::format_context &ctx) const {
    return std::format_to(ctx.out(), R"({{"n_meth": {}, "n_unmeth": {}}})",
                          l.n_meth, l.n_unmeth);
  }
};

template <>
struct std::formatter<transferase::level_element_covered_t>
  : std::formatter<std::string> {
  auto
  format(const transferase::level_element_covered_t &l,
         std::format_context &ctx) const {
    return std::format_to(
      ctx.out(), R"({{"n_meth": {}, "n_unmeth": {}, "n_covered": {}}})",
      l.n_meth, l.n_unmeth, l.n_covered);
  }
};

#endif  // LIB_LEVEL_ELEMENT_HPP_
