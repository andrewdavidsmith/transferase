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

#include "intervals_writer.hpp"

#include "genome_index.hpp"
#include "genome_index_metadata.hpp"
#include "genomic_interval.hpp"
#include "level_container.hpp"
#include "level_container_md.hpp"  // IWYU: pragma: keep
#include "level_element.hpp"

#include <algorithm>  // for transform
#include <cerrno>
#include <format>
#include <fstream>
#include <iterator>  // for std::size, std::cbegin
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>  // for std::is_same_v
#include <vector>

namespace transferase {

[[nodiscard]] static inline auto
write_bedlike_intervals_impl(const std::string &outfile,
                             const genome_index_metadata &meta,
                             const std::vector<genomic_interval> &intervals,
                             const auto &levels,
                             bool classic_format) noexcept -> std::error_code {
  const auto lvl_to_string = [classic_format](const auto &l) {
    return classic_format ? l.tostring_classic() : l.tostring_counts();
  };

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto n_levels = std::size(levels);
  auto prev_ch_id = genomic_interval::not_a_chrom;
  std::string chrom_name;
  for (const auto [i, interval] : std::views::enumerate(intervals)) {
    if (interval.ch_id != prev_ch_id) {
      chrom_name = meta.chrom_order[interval.ch_id];
      prev_ch_id = interval.ch_id;
    }
    std::print(out, "{}\t{}\t{}", chrom_name, interval.start, interval.stop);
    for (auto j = 0u; j < n_levels; ++j)
      std::print(out, "\t{}", lvl_to_string(levels[j][i]));
    std::println(out);
  }
  return {};
}

template <typename level_element>
[[nodiscard]] static inline auto
write_bedlike_intervals_impl(const std::string &outfile,
                             const genome_index_metadata &meta,
                             const std::vector<genomic_interval> &intervals,
                             const level_container_md<level_element> &levels,
                             bool classic_format) noexcept -> std::error_code {
  const auto lvl_to_string = [classic_format](const auto &l) {
    return classic_format ? l.tostring_classic() : l.tostring_counts();
  };

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto n_levels = levels.n_cols;
  auto prev_ch_id = genomic_interval::not_a_chrom;
  std::string chrom_name;
  for (const auto [i, interval] : std::views::enumerate(intervals)) {
    if (interval.ch_id != prev_ch_id) {
      chrom_name = meta.chrom_order[interval.ch_id];
      prev_ch_id = interval.ch_id;
    }
    std::print(out, "{}\t{}\t{}", chrom_name, interval.start, interval.stop);
    for (auto j = 0u; j < n_levels; ++j)
      std::print(out, "\t{}", lvl_to_string(levels[i, j]));
    std::println(out);
  }
  return {};
}

[[nodiscard]] static inline auto
write_intervals_dataframe_scores_impl(
  const std::string &outfile, const std::vector<std::string> &names,
  const genome_index_metadata &meta,
  const std::vector<genomic_interval> &intervals, const std::uint32_t min_reads,
  const auto &levels, const char rowname_delim,
  const bool write_header) -> std::error_code {
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto none_label = "NA"sv;
  // static constexpr auto delim = '\t';

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  if (write_header) {
    const auto joined =
      names | std::views::join_with('\t') | std::ranges::to<std::string>();
    std::println(out, "{}", joined);
  }

  const auto n_levels = std::size(levels);
  auto prev_ch_id = genomic_interval::not_a_chrom;
  std::string chrom_name;
  for (const auto [i, interval] : std::views::enumerate(intervals)) {
    if (interval.ch_id != prev_ch_id) {
      chrom_name = meta.chrom_order[interval.ch_id];
      prev_ch_id = interval.ch_id;
    }
    std::print(out, "{}{}{}{}{}", chrom_name, rowname_delim, interval.start,
               rowname_delim, interval.stop);
    for (auto j = 0u; j < n_levels; ++j)
      if (levels[j][i].n_reads() >= min_reads)
        std::print(out, "\t{:.6}", levels[j][i].get_wmean());
      else
        std::print(out, "\t{}", none_label);
    std::println(out);
  }
  return {};
}

template <typename level_element>
[[nodiscard]] static inline auto
write_intervals_dataframe_scores_impl(
  const std::string &outfile, const std::vector<std::string> &names,
  const genome_index_metadata &meta,
  const std::vector<genomic_interval> &intervals, const std::uint32_t min_reads,
  const level_container_md<level_element> &levels, const char rowname_delim,
  const bool write_header) -> std::error_code {
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto none_label = "NA"sv;
  // static constexpr auto delim = '\t';

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  if (write_header) {
    const auto joined =
      names | std::views::join_with('\t') | std::ranges::to<std::string>();
    std::println(out, "{}", joined);
  }

  const auto n_levels = levels.n_cols;
  auto prev_ch_id = genomic_interval::not_a_chrom;
  std::string chrom_name;
  for (const auto [i, interval] : std::views::enumerate(intervals)) {
    if (interval.ch_id != prev_ch_id) {
      chrom_name = meta.chrom_order[interval.ch_id];
      prev_ch_id = interval.ch_id;
    }
    std::print(out, "{}{}{}{}{}", chrom_name, rowname_delim, interval.start,
               rowname_delim, interval.stop);
    for (auto j = 0u; j < n_levels; ++j)
      if (levels[i, j].n_reads() >= min_reads)
        std::print(out, "\t{:.6}", levels[i, j].get_wmean());
      else
        std::print(out, "\t{}", none_label);
    std::println(out);
  }
  return {};
}

[[nodiscard]] static inline auto
write_intervals_dataframe_impl(const std::string &outfile,
                               const std::vector<std::string> &names,
                               const genome_index_metadata &meta,
                               const std::vector<genomic_interval> &intervals,
                               const auto &levels) -> std::error_code {
  // static constexpr auto delim = '\t';  // not optional for now
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

  const auto n_levels = std::size(levels);
  auto prev_ch_id = genomic_interval::not_a_chrom;
  std::string chrom_name;
  for (const auto [i, interval] : std::views::enumerate(intervals)) {
    if (interval.ch_id != prev_ch_id) {
      chrom_name = meta.chrom_order[interval.ch_id];
      prev_ch_id = interval.ch_id;
    }
    std::print(out, "{}.{}.{}", chrom_name, interval.start, interval.stop);
    for (auto j = 0u; j < n_levels; ++j)
      std::print(out, "\t{}", levels[j][i].tostring_counts());
    std::println(out);
  }
  return {};
}

template <typename level_element>
[[nodiscard]] static inline auto
write_intervals_dataframe_impl(
  const std::string &outfile, const std::vector<std::string> &names,
  const genome_index_metadata &meta,
  const std::vector<genomic_interval> &intervals,
  const level_container_md<level_element> &levels) -> std::error_code {
  // static constexpr auto delim = '\t';  // not optional for now
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

  const auto n_levels = levels.n_cols;
  auto prev_ch_id = genomic_interval::not_a_chrom;
  std::string chrom_name;
  for (const auto [i, interval] : std::views::enumerate(intervals)) {
    if (interval.ch_id != prev_ch_id) {
      chrom_name = meta.chrom_order[interval.ch_id];
      prev_ch_id = interval.ch_id;
    }
    std::print(out, "{}.{}.{}", chrom_name, interval.start, interval.stop);
    for (auto j = 0u; j < n_levels; ++j)
      std::print(out, "\t{}", levels[i, j].tostring_counts());
    std::println(out);
  }
  return {};
}

template <>
[[nodiscard]] auto
intervals_writer::write_bedlike_impl(
  const std::vector<level_container<level_element_covered_t>> &levels,
  const bool classic_format) const noexcept -> std::error_code {
  return write_bedlike_intervals_impl(outfile, index.get_metadata(), intervals,
                                      levels, classic_format);
}

template <>
[[nodiscard]] auto
intervals_writer::write_bedlike_impl(
  const std::vector<level_container<level_element_t>> &levels,
  const bool classic_format) const noexcept -> std::error_code {
  return write_bedlike_intervals_impl(outfile, index.get_metadata(), intervals,
                                      levels, classic_format);
}

template <>
[[nodiscard]] auto
intervals_writer::write_bedlike_impl(
  const level_container_md<level_element_t> &levels,
  const bool classic_format) const noexcept -> std::error_code {
  return write_bedlike_intervals_impl(outfile, index.get_metadata(), intervals,
                                      levels, classic_format);
}

template <>
[[nodiscard]] auto
intervals_writer::write_bedlike_impl(
  const level_container_md<level_element_covered_t> &levels,
  const bool classic_format) const noexcept -> std::error_code {
  return write_bedlike_intervals_impl(outfile, index.get_metadata(), intervals,
                                      levels, classic_format);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_scores_impl(
  const std::vector<level_container<level_element_t>> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_intervals_dataframe_scores_impl(
    outfile, names, index.get_metadata(), intervals, min_reads, levels,
    rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_scores_impl(
  const std::vector<level_container<level_element_covered_t>> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_intervals_dataframe_scores_impl(
    outfile, names, index.get_metadata(), intervals, min_reads, levels,
    rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_scores_impl(
  const level_container_md<level_element_t> &levels, const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_intervals_dataframe_scores_impl(
    outfile, names, index.get_metadata(), intervals, min_reads, levels,
    rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_scores_impl(
  const level_container_md<level_element_covered_t> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_intervals_dataframe_scores_impl(
    outfile, names, index.get_metadata(), intervals, min_reads, levels,
    rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_impl(
  const std::vector<level_container<level_element_t>> &levels) const noexcept
  -> std::error_code {
  return write_intervals_dataframe_impl(outfile, names, index.get_metadata(),
                                        intervals, levels);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_impl(
  const std::vector<level_container<level_element_covered_t>> &levels)
  const noexcept -> std::error_code {
  return write_intervals_dataframe_impl(outfile, names, index.get_metadata(),
                                        intervals, levels);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_impl(
  const level_container_md<level_element_t> &levels) const noexcept
  -> std::error_code {
  return write_intervals_dataframe_impl(outfile, names, index.get_metadata(),
                                        intervals, levels);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_impl(
  const level_container_md<level_element_covered_t> &levels) const noexcept
  -> std::error_code {
  return write_intervals_dataframe_impl(outfile, names, index.get_metadata(),
                                        intervals, levels);
}

}  // namespace transferase
