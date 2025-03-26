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
#include "level_container_md.hpp"  // IWYU pragma: keep
#include "level_element.hpp"
#include "level_element_formatter.hpp"

#include <algorithm>  // IWYU pragma: keep (for transform)
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
                             const std::vector<std::uint32_t> &n_cpgs,
                             const auto &levels,
                             bool classic_format) noexcept -> std::error_code {
  const auto lvl_to_string = [classic_format](const auto &l) {
    return classic_format ? l.tostring_classic() : l.tostring_counts();
  };

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto write_n_cpgs = !n_cpgs.empty();

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
    if (write_n_cpgs)
      std::print(out, "\t{}", n_cpgs[i]);
    std::println(out);
  }
  return {};
}

[[nodiscard]] static inline auto
write_intervals_dataframe_scores_impl(
  const std::string &outfile, const std::vector<std::string> &names,
  const genome_index_metadata &meta,
  const std::vector<genomic_interval> &intervals, const std::uint32_t min_reads,
  const std::vector<std::uint32_t> &n_cpgs, const auto &levels,
  const char rowname_delim, const bool write_header) -> std::error_code {
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto none_label = "NA"sv;
  // static constexpr auto delim = '\t';

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto write_n_cpgs = !n_cpgs.empty();

  if (write_header) {
    auto joined =
      names | std::views::join_with('\t') | std::ranges::to<std::string>();
    if (write_n_cpgs)
      joined += "\tN_CPG";
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
    if (write_n_cpgs)
      std::print(out, "\t{}", n_cpgs[i]);
    std::println(out);
  }
  return {};
}

[[nodiscard]] static inline auto
write_intervals_dataframe_impl(
  // clang-format off
  const std::string &outfile,
  const std::vector<std::string> &names,
  const genome_index_metadata &meta,
  const std::vector<genomic_interval> &intervals,
  const std::vector<std::uint32_t> &n_cpgs,
  const auto &levels,
  const char rowname_delim,
  const bool write_header
  // clang-format on
  ) -> std::error_code {
  static constexpr auto delim = '\t';  // not optional for now
  // determine type
  using outer_type = typename std::remove_cvref_t<decltype(levels)>::value_type;
  using level_element = typename std::remove_cvref_t<outer_type>::value_type;
  const auto hdr_formatter = [&](const auto &r) {
    return std::format(level_element::hdr_fmt, r, delim, r, delim, r);
  };

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto write_n_cpgs = !n_cpgs.empty();

  if (write_header) {
    auto joined = names | std::views::transform(hdr_formatter) |
                  std::views::join_with(delim) | std::ranges::to<std::string>();
    if (write_n_cpgs)
      joined += std::format("{}{}", delim, "N_CPG");
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
      std::print(out, "{}{}", delim, levels[j][i].tostring_counts());
    if (write_n_cpgs)
      std::print(out, "{}{}", delim, n_cpgs[i]);
    std::println(out);
  }
  return {};
}

// ADS: functions taking level_container below

template <typename level_element>
[[nodiscard]] static inline auto
write_bedlike_intervals_impl(
  // clang-format off
  const std::string &outfile,
  const genome_index_metadata &meta,
  const std::vector<genomic_interval> &intervals,
  const std::vector<std::uint32_t> &n_cpgs,
  const level_container_md<level_element> &levels,
  const level_element_mode mode
  // clang-format on
  ) noexcept -> std::error_code {
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto rowname_delim{'\t'};
  static constexpr auto delim{'\t'};
  static constexpr auto newline{'\n'};

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto write_n_cpgs = !n_cpgs.empty();

  std::vector<char> line(intervals_writer::output_buffer_size);
  auto line_beg = line.data();
  // NOLINTNEXTLINE (*-pointer-arithmetic)
  const auto line_end = line.data() + std::size(line);
  auto error = std::errc{};

  const auto n_levels = levels.n_cols;
  auto prev_ch_id = genomic_interval::not_a_chrom;
  std::string chrom_name;

  for (const auto [i, interval] : std::views::enumerate(intervals)) {
    if (interval.ch_id != prev_ch_id) {
      chrom_name = meta.chrom_order[interval.ch_id];
      prev_ch_id = interval.ch_id;
      line_beg = line.data();
      push_buffer(line_beg, line_end, error, chrom_name, rowname_delim);
    }
    auto cursor = line_beg;
    push_buffer(cursor, line_end, error, interval.start, rowname_delim,
                interval.stop);

    for (const auto j : std::views::iota(0u, n_levels))
      push_buffer_elem(cursor, line_end, error, levels[i, j], mode, delim);

    if (write_n_cpgs)
      push_buffer(cursor, line_end, error, delim, n_cpgs[i]);
    push_buffer(cursor, line_end, error, newline);
    out.write(line.data(), std::distance(line.data(), cursor));
  }
  return {};
}

template <typename level_element>
[[nodiscard]] static inline auto
write_intervals_dataframe_scores_impl(
  // clang-format off
  const std::string &outfile,
  const std::vector<std::string> &names,
  const genome_index_metadata &meta,
  const std::vector<genomic_interval> &intervals,
  const std::uint32_t min_reads,
  const std::vector<std::uint32_t> &n_cpgs,
  const level_container_md<level_element> &levels,
  const char rowname_delim,
  const bool write_header
  // clang-format on
  ) -> std::error_code {
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto delim{'\t'};
  static constexpr auto newline{'\n'};
  static constexpr auto none_label = "NA"sv;

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto write_n_cpgs = !n_cpgs.empty();

  if (write_header) {
    auto joined =
      names | std::views::join_with(delim) | std::ranges::to<std::string>();
    if (write_n_cpgs)
      joined += std::format("{}{}", delim, "N_CPG"sv);
    std::println(out, "{}", joined);
  }

  std::vector<char> line(intervals_writer::output_buffer_size);
  auto line_beg = line.data();
  // NOLINTNEXTLINE (*-pointer-arithmetic)
  const auto line_end = line.data() + std::size(line);
  auto error = std::errc{};

  const auto n_levels = levels.n_cols;
  auto prev_ch_id = genomic_interval::not_a_chrom;
  std::string chrom_name;
  for (const auto [i, interval] : std::views::enumerate(intervals)) {
    if (interval.ch_id != prev_ch_id) {
      chrom_name = meta.chrom_order[interval.ch_id];
      prev_ch_id = interval.ch_id;
      line_beg = line.data();
      push_buffer(line_beg, line_end, error, chrom_name, rowname_delim);
    }
    auto cursor = line_beg;
    push_buffer(cursor, line_end, error, interval.start, rowname_delim,
                interval.stop);
    for (const auto j : std::views::iota(0u, n_levels))
      push_buffer_score(cursor, line_end, error, levels[i, j], none_label,
                        min_reads, delim);
    if (write_n_cpgs)
      push_buffer(cursor, line_end, error, delim, n_cpgs[i]);
    push_buffer(cursor, line_end, error, newline);
    out.write(line.data(), std::distance(line.data(), cursor));
  }
  return {};
}

template <typename level_element>
[[nodiscard]] static inline auto
write_intervals_dataframe_impl(
  // clang-format off
  const std::string &outfile,
  const std::vector<std::string> &names,
  const genome_index_metadata &meta,
  const std::vector<genomic_interval> &intervals,
  const std::vector<std::uint32_t> &n_cpgs,
  const level_container_md<level_element> &levels,
  const char rowname_delim,
  const bool write_header
  // clang-format on
  ) -> std::error_code {
  static constexpr auto delim{'\t'};
  static constexpr auto newline{'\n'};

  const auto hdr_formatter = [&](const auto &r) {
    return std::format(level_element::hdr_fmt, r, delim, r, delim, r);
  };

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto write_n_cpgs = !n_cpgs.empty();

  if (write_header) {
    auto joined = names | std::views::transform(hdr_formatter) |
                  std::views::join_with(delim) | std::ranges::to<std::string>();
    if (write_n_cpgs)
      joined += std::format("{}{}", delim, "N_CPG");
    std::println(out, "{}", joined);
  }

  std::vector<char> line(intervals_writer::output_buffer_size);
  auto line_beg = line.data();
  // NOLINTNEXTLINE (*-pointer-arithmetic)
  const auto line_end = line.data() + std::size(line);
  auto error = std::errc{};

  const auto n_levels = levels.n_cols;
  auto prev_ch_id = genomic_interval::not_a_chrom;
  std::string chrom_name;

  for (const auto [i, interval] : std::views::enumerate(intervals)) {
    if (interval.ch_id != prev_ch_id) {
      chrom_name = meta.chrom_order[interval.ch_id];
      prev_ch_id = interval.ch_id;
      line_beg = line.data();
      push_buffer(line_beg, line_end, error, chrom_name, rowname_delim);
    }
    auto cursor = line_beg;
    push_buffer(cursor, line_end, error, interval.start, rowname_delim,
                interval.stop);
    for (auto j = 0u; j < n_levels; ++j)
      push_buffer_elem(cursor, line_end, error, levels[i, j],
                       level_element_mode::counts, delim);

    if (write_n_cpgs)
      push_buffer(cursor, line_end, error, delim, n_cpgs[i]);
    push_buffer(cursor, line_end, error, newline);
    out.write(line.data(), std::distance(line.data(), cursor));
  }
  return {};
}

// ADS: Pseudo-declarations below

template <>
[[nodiscard]] auto
intervals_writer::write_bedlike_impl(
  const std::vector<level_container<level_element_covered_t>> &levels,
  const bool classic_format) const noexcept -> std::error_code {
  return write_bedlike_intervals_impl(outfile, index.get_metadata(), intervals,
                                      n_cpgs, levels, classic_format);
}

template <>
[[nodiscard]] auto
intervals_writer::write_bedlike_impl(
  const std::vector<level_container<level_element_t>> &levels,
  const bool classic_format) const noexcept -> std::error_code {
  return write_bedlike_intervals_impl(outfile, index.get_metadata(), intervals,
                                      n_cpgs, levels, classic_format);
}

template <>
[[nodiscard]] auto
intervals_writer::write_bedlike_impl(
  const level_container_md<level_element_t> &levels,
  const bool classic_format) const noexcept -> std::error_code {
  return write_bedlike_intervals_impl(
    outfile, index.get_metadata(), intervals, n_cpgs, levels,
    classic_format ? level_element_mode::classic : level_element_mode::counts);
}

template <>
[[nodiscard]] auto
intervals_writer::write_bedlike_impl(
  const level_container_md<level_element_covered_t> &levels,
  const bool classic_format) const noexcept -> std::error_code {
  return write_bedlike_intervals_impl(
    outfile, index.get_metadata(), intervals, n_cpgs, levels,
    classic_format ? level_element_mode::classic : level_element_mode::counts);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_scores_impl(
  const std::vector<level_container<level_element_t>> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_intervals_dataframe_scores_impl(
    outfile, names, index.get_metadata(), intervals, min_reads, n_cpgs, levels,
    rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_scores_impl(
  const std::vector<level_container<level_element_covered_t>> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_intervals_dataframe_scores_impl(
    outfile, names, index.get_metadata(), intervals, min_reads, n_cpgs, levels,
    rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_scores_impl(
  const level_container_md<level_element_t> &levels, const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_intervals_dataframe_scores_impl(
    outfile, names, index.get_metadata(), intervals, min_reads, n_cpgs, levels,
    rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_scores_impl(
  const level_container_md<level_element_covered_t> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_intervals_dataframe_scores_impl(
    outfile, names, index.get_metadata(), intervals, min_reads, n_cpgs, levels,
    rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_impl(
  const std::vector<level_container<level_element_t>> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_intervals_dataframe_impl(outfile, names, index.get_metadata(),
                                        intervals, n_cpgs, levels,
                                        rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_impl(
  const std::vector<level_container<level_element_covered_t>> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_intervals_dataframe_impl(outfile, names, index.get_metadata(),
                                        intervals, n_cpgs, levels,
                                        rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_impl(
  const level_container_md<level_element_t> &levels, const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_intervals_dataframe_impl(outfile, names, index.get_metadata(),
                                        intervals, n_cpgs, levels,
                                        rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_impl(
  const level_container_md<level_element_covered_t> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_intervals_dataframe_impl(outfile, names, index.get_metadata(),
                                        intervals, n_cpgs, levels,
                                        rowname_delim, write_header);
}

}  // namespace transferase
