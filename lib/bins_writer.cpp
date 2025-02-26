/* MIT License
 *
 * Copyright (c) 2025 Andrew D Smith
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

#include "bins_writer.hpp"

#include "genome_index.hpp"
#include "genome_index_metadata.hpp"
#include "level_container.hpp"
#include "level_element.hpp"

#include <algorithm>  // std::min
#include <array>
#include <cassert>
#include <cerrno>
#include <charconv>
#include <cstdint>  // for std::uint32_t
#include <format>
#include <fstream>
#include <iterator>  // for std::size, std::cbegin, std::cend
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>  // for std::is_same_v
#include <vector>

namespace transferase {

[[nodiscard]] static inline auto
write_bedlike_bins_impl(const std::string &outfile,
                        const genome_index_metadata &meta,
                        const std::uint32_t bin_size, const auto &levels,
                        const bool classic_format) -> std::error_code {
  const auto lvl_to_string = [classic_format](const auto &l) {
    return classic_format ? l.tostring_classic() : l.tostring_counts();
  };

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto n_levels = std::size(levels);
  std::uint32_t i = 0;
  const auto zipped = std::views::zip(meta.chrom_size, meta.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    for (std::uint32_t bin_beg = 0; bin_beg < chrom_size; bin_beg += bin_size) {
      const auto bin_end = std::min(bin_beg + bin_size, chrom_size);
      std::print(out, "{}\t{}\t{}", chrom_name, bin_beg, bin_end);
      for (auto j = 0u; j < n_levels; ++j)
        std::print(out, "\t{}", lvl_to_string(levels[j][i]));
      std::println(out);
      ++i;
    }
  }
  return {};
}

[[nodiscard]] static inline auto
write_bins_dataframe_scores_impl(const std::string &outfile,
                                 const std::vector<std::string> &names,
                                 const genome_index_metadata &meta,
                                 const std::uint32_t bin_size,
                                 const std::uint32_t min_reads,
                                 const auto &levels, const char rowname_delim,
                                 const bool write_header) -> std::error_code {
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto none_label = "NA"sv;
  static constexpr auto delim{'\t'};

  const auto get_score = [](const auto x) {
    const double total = x.n_meth + x.n_unmeth;
    return x.n_meth / std::max(1.0, total);
  };

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  if (write_header) {
    const auto joined =
      names | std::views::join_with(delim) | std::ranges::to<std::string>();
    std::println(out, "{}", joined);
  }

  const auto n_levels = std::ssize(levels);
  std::uint32_t i = 0;
  const auto zipped = std::views::zip(meta.chrom_size, meta.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    for (std::uint32_t bin_beg = 0; bin_beg < chrom_size; bin_beg += bin_size) {
      const auto bin_end = std::min(bin_beg + bin_size, chrom_size);
      std::print(out, "{}{}{}{}{}", chrom_name, rowname_delim, bin_beg,
                 rowname_delim, bin_end);
      for (auto j = 0; j < n_levels; ++j)
        if (levels[j][i].n_reads() >= min_reads)
          std::print(out, "{}{:.6}", delim, get_score(levels[j][i]));
        else
          std::print(out, "{}{}", delim, none_label);
      std::println(out);
      ++i;
    }
  }
  return {};
}

[[nodiscard]] static inline auto
write_bins_dataframe_impl(const std::string &outfile,
                          const std::vector<std::string> &names,
                          const genome_index_metadata &meta,
                          const std::uint32_t bin_size,
                          const auto &levels) -> std::error_code {
  // static constexpr auto delim = '\t'; // not optional for now
  using outer_type = typename std::remove_cvref_t<decltype(levels)>::value_type;
  using level_element = typename std::remove_cvref_t<outer_type>::value_type;
  const auto hdr_formatter = [&](const auto &r) {
    return std::format(level_element::hdr_fmt, r, r, r);
  };

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto joined = names | std::views::transform(hdr_formatter) |
                      std::views::join_with('\t') |
                      std::ranges::to<std::string>();
  std::println(out, "{}", joined);

  const auto n_levels = std::ssize(levels);
  std::uint32_t i = 0;
  const auto zipped = std::views::zip(meta.chrom_size, meta.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped)
    for (std::uint32_t bin_beg = 0; bin_beg < chrom_size; bin_beg += bin_size) {
      const auto bin_end = std::min(bin_beg + bin_size, chrom_size);
      std::print(out, "{}.{}.{}", chrom_name, bin_beg, bin_end);
      for (auto j = 0; j < n_levels; ++j)
        std::print(out, "\t{}", levels[j][i].tostring_counts());
      std::println(out);
      ++i;
    }

  return {};
}

template <>
[[nodiscard]] auto
bins_writer::write_dataframe_scores_impl(
  const std::vector<level_container<level_element_t>> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_bins_dataframe_scores_impl(outfile, names, index.get_metadata(),
                                          bin_size, min_reads, levels,
                                          rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
bins_writer::write_dataframe_scores_impl(
  const std::vector<level_container<level_element_covered_t>> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_bins_dataframe_scores_impl(outfile, names, index.get_metadata(),
                                          bin_size, min_reads, levels,
                                          rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
bins_writer::write_dataframe_impl(
  const std::vector<level_container<level_element_t>> &levels) const noexcept
  -> std::error_code {
  return write_bins_dataframe_impl(outfile, names, index.get_metadata(),
                                   bin_size, levels);
}

template <>
[[nodiscard]] auto
bins_writer::write_dataframe_impl(
  const std::vector<level_container<level_element_covered_t>> &levels)
  const noexcept -> std::error_code {
  return write_bins_dataframe_impl(outfile, names, index.get_metadata(),
                                   bin_size, levels);
}

template <>
[[nodiscard]] auto
bins_writer::write_bedlike_impl(
  const std::vector<level_container<level_element_t>> &levels,
  const bool classic_format) const noexcept -> std::error_code {
  return write_bedlike_bins_impl(outfile, index.get_metadata(), bin_size,
                                 levels, classic_format);
}

template <>
[[nodiscard]] auto
bins_writer::write_bedlike_impl(
  const std::vector<level_container<level_element_covered_t>> &levels,
  const bool classic_format) const noexcept -> std::error_code {
  return write_bedlike_bins_impl(outfile, index.get_metadata(), bin_size,
                                 levels, classic_format);
}

}  // namespace transferase
