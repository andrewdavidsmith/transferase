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
#include "level_container.hpp"  // IWYU pragma: keep
#include "level_container_flat.hpp"
#include "level_element.hpp"
#include "level_element_formatter.hpp"

#include "macos_helper.hpp"

#include <algorithm>
#include <cerrno>
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

template <typename level_element>
[[nodiscard]] static auto
get_empty_row(const bool write_empty, const auto n_levels, const auto delim,
              const level_element_mode mode) -> std::string {
  if (!write_empty)
    return {};

  std::vector<char> line(bins_writer::output_buffer_size);
  // NOLINTNEXTLINE (*-pointer-arithmetic)
  const auto line_end = line.data() + std::size(line);
  auto cursor = line.data();
  auto error = std::errc{};
  const level_element empty_elem{};

  for (const auto _ : std::views::iota(0u, n_levels))
    level_format::push_elem(cursor, line_end, error, empty_elem, mode, delim);
  return std::string(line.data(), std::distance(line.data(), cursor));
}

template <typename level_element>
[[nodiscard]] static auto
get_empty_row_scores(const bool write_empty, const auto n_levels,
                     const auto delim, const auto none_label) -> std::string {
  static constexpr auto min_reads = 1u;  // there will never be a read here
  if (!write_empty)
    return {};

  std::vector<char> line(bins_writer::output_buffer_size);
  // NOLINTNEXTLINE (*-pointer-arithmetic)
  const auto line_end = line.data() + std::size(line);
  auto cursor = line.data();
  auto error = std::errc{};
  const level_element empty_elem{};

  for (const auto _ : std::views::iota(0u, n_levels))
    level_format::push_score(cursor, line_end, error, empty_elem, none_label,
                             min_reads, delim);
  return std::string(line.data(), std::distance(line.data(), cursor));
}

// ADS: first functions that accept 2D vector

[[nodiscard]] static inline auto
write_bedlike_bins_impl(
  // clang-format off
  const std::string &outfile,
  const genome_index &index,
  const std::uint32_t bin_size,
  const auto &levels,
  const level_element_mode mode,
  const bool write_n_cpgs,
  const bool write_empty,
  const std::vector<std::uint32_t> &n_cpgs
  // clang-format on
  ) -> std::error_code {
  static constexpr auto delim{'\t'};
  const auto lvl_to_string = [mode](const auto &l) {
    return mode == level_element_mode::classic ? l.tostring_classic()
                                               : l.tostring_counts();
  };

  // determine type
  using outer_type = typename std::remove_cvref_t<decltype(levels)>::value_type;
  using level_element = typename std::remove_cvref_t<outer_type>::value_type;

  const auto n_levels = std::size(levels);
  const auto empty_row =
    get_empty_row<level_element>(write_empty, n_levels, delim, mode);

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  std::uint32_t bin_idx = 0;     // index for all bins
  std::uint32_t ne_bin_idx = 0;  // index for bins that can be non-empty

  const auto &meta = index.get_metadata();
  const auto zipped = std::views::zip(meta.chrom_size, meta.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    for (std::uint32_t bin_beg = 0; bin_beg < chrom_size; bin_beg += bin_size) {
      if (n_cpgs[bin_idx] > 0) {
        const auto bin_end = std::min(bin_beg + bin_size, chrom_size);
        std::print(out, "{}{}{}{}{}", chrom_name, delim, bin_beg, delim,
                   bin_end);
        for (const auto col_idx : std::views::iota(0u, n_levels))
          std::print(out, "{}{}", delim,
                     lvl_to_string(levels[col_idx][ne_bin_idx]));
        if (write_n_cpgs)
          std::print(out, "{}{}", delim, n_cpgs[bin_idx]);
        /* println() without arg doesn't work on macos clang */
        // std::println(out);
        std::print(out, "\n");
        ++ne_bin_idx;
      }
      else if (write_empty) {
        const auto bin_end = std::min(bin_beg + bin_size, chrom_size);
        std::print(out, "{}{}{}{}{}{}", chrom_name, delim, bin_beg, delim,
                   bin_end, empty_row);
        if (write_n_cpgs)
          std::print(out, "{}0", delim);
        /* println() without arg doesn't work on macos clang */
        // std::println(out);
        std::print(out, "\n");
      }
      ++bin_idx;
    }
  }
  return {};
}

[[nodiscard]] static inline auto
write_bins_dfscores_impl(
  // clang-format off
  const std::string &outfile,
  const std::vector<std::string> &names,
  const genome_index &index,
  const std::uint32_t bin_size,
  const std::uint32_t min_reads,
  const auto &levels,
  const char rowname_delim,
  const bool write_header,
  const bool write_n_cpgs,
  const bool write_empty,
  const std::vector<std::uint32_t> &n_cpgs
  // clang-format on
  ) -> std::error_code {
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto none_label = "NA"sv;
  static constexpr auto delim{'\t'};

  // determine type
  using outer_type = typename std::remove_cvref_t<decltype(levels)>::value_type;
  using level_element = typename std::remove_cvref_t<outer_type>::value_type;

  const auto n_levels = std::size(levels);
  const auto empty_row = get_empty_row_scores<level_element>(
    write_empty, n_levels, delim, none_label);

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  if (write_header) {
    const auto hdr_fmtr = [&](const auto &r) {
      return std::format(level_element::hdr_fmt_scr, r, delim, r);
    };
    auto joined = join_with(names | std::views::transform(hdr_fmtr), delim);
    if (write_n_cpgs)
      joined += std::format("{}{}", delim, "N_CPG"sv);
    std::println(out, "{}", joined);
  }

  std::uint32_t bin_idx = 0;     // index for all bins
  std::uint32_t ne_bin_idx = 0;  // index for bins that can be non-empty

  const auto &meta = index.get_metadata();
  const auto zipped = std::views::zip(meta.chrom_size, meta.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    for (std::uint32_t bin_beg = 0; bin_beg < chrom_size; bin_beg += bin_size) {
      if (n_cpgs[bin_idx] > 0) {
        std::print(out, "{}{}{}", chrom_name, rowname_delim, bin_beg);
        for (const auto col_idx : std::views::iota(0u, n_levels))
          if (levels[col_idx][ne_bin_idx].n_reads() >= min_reads)
            std::print(out, "{}{:.6}", delim,
                       levels[col_idx][ne_bin_idx].get_wmean());
          else
            std::print(out, "{}{}", delim, none_label);
        /* println() without arg doesn't work on macos clang */
        // std::println(out);
        std::print(out, "\n");
        ++ne_bin_idx;
      }
      else if (write_empty)
        std::println(out, "{}{}{}{}", chrom_name, rowname_delim, bin_beg,
                     empty_row);
      ++bin_idx;
    }
  }
  return {};
}

[[nodiscard]] static inline auto
write_bins_dataframe_impl(
  // clang-format off
  const std::string &outfile,
  const std::vector<std::string> &names,
  const genome_index &index,
  const std::uint32_t bin_size,
  const auto &levels,
  const level_element_mode mode,
  const char rowname_delim,
  const bool write_header,
  const bool write_n_cpgs,
  const bool write_empty,
  const std::vector<std::uint32_t> &n_cpgs
  // clang-format on
  ) -> std::error_code {
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto delim = '\t';  // not optional for now

  // determine type
  using outer_type = typename std::remove_cvref_t<decltype(levels)>::value_type;
  using level_element = typename std::remove_cvref_t<outer_type>::value_type;

  const auto n_levels = std::size(levels);
  const auto empty_row =
    get_empty_row<level_element>(write_empty, n_levels, delim, mode);

  const auto lvl_to_string = [mode](const auto &l) {
    return mode == level_element_mode::classic ? l.tostring_classic()
                                               : l.tostring_counts();
  };

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  if (write_header) {
    const auto hdr_fmtr = [&](const auto &r) {
      return mode == level_element_mode::classic
               ? std::format(level_element::hdr_fmt_cls, r, delim, r, delim, r)
               : std::format(level_element::hdr_fmt, r, delim, r, delim, r);
    };
    auto joined = join_with(names | std::views::transform(hdr_fmtr), delim);
    if (write_n_cpgs)
      joined += std::format("{}{}", delim, "N_CPG"sv);
    std::println(out, "{}", joined);
  }

  std::uint32_t bin_idx = 0;     // index for all bins
  std::uint32_t ne_bin_idx = 0;  // index for bins that can be non-empty

  const auto &meta = index.get_metadata();
  const auto zipped = std::views::zip(meta.chrom_size, meta.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped)
    for (std::uint32_t bin_beg = 0; bin_beg < chrom_size; bin_beg += bin_size) {
      if (n_cpgs[bin_idx] > 0) {
        std::print(out, "{}{}{}", chrom_name, rowname_delim, bin_beg);
        for (const auto col_idx : std::views::iota(0u, n_levels))
          std::print(out, "{}{}", delim,
                     lvl_to_string(levels[col_idx][ne_bin_idx]));
        if (write_n_cpgs)
          std::print(out, "{}{}", delim, n_cpgs[bin_idx]);
        /* println() without arg doesn't work on macos clang */
        // std::println(out);
        std::print(out, "\n");
        ++ne_bin_idx;
      }
      else if (write_empty)
        std::println(out, "{}{}{}{}", chrom_name, rowname_delim, bin_beg,
                     empty_row);
      ++bin_idx;
    }

  return {};
}

// ADS: now functions that accept level_container

template <typename level_element>
[[nodiscard]] static inline auto
write_bedlike_bins_impl(
  // clang-format off
  const std::string &outfile,
  const genome_index &index,
  const std::uint32_t bin_size,
  const level_container<level_element> &levels,
  const level_element_mode mode,
  const bool write_n_cpgs,
  const bool write_empty,
  const std::vector<std::uint32_t> &n_cpgs
  // clang-format on
  ) -> std::error_code {
  static constexpr auto rowname_delim{'\t'};
  static constexpr auto delim{'\t'};
  static constexpr auto newline{'\n'};

  const auto n_levels = levels.n_cols;
  const auto empty_row =
    get_empty_row<level_element>(write_empty, n_levels, delim, mode);
  const auto empty_row_size = std::size(empty_row);

  std::vector<char> line(bins_writer::output_buffer_size);
  // NOLINTNEXTLINE (*-pointer-arithmetic)
  const auto line_end = line.data() + std::size(line);
  auto error = std::errc{};

  // open the output file
  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  std::uint32_t bin_idx = 0;     // index for all bins
  std::uint32_t ne_bin_idx = 0;  // index for bins that can be non-empty

  const auto &meta = index.get_metadata();
  const auto zipped = std::views::zip(meta.chrom_size, meta.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    auto line_beg = line.data();
    level_format::push(line_beg, line_end, error, chrom_name, rowname_delim);

    for (std::uint32_t bin_beg = 0; bin_beg < chrom_size; bin_beg += bin_size) {
      if (n_cpgs[bin_idx] > 0) {
        const auto bin_end = std::min(bin_beg + bin_size, chrom_size);
        auto cursor = line_beg;
        level_format::push(cursor, line_end, error, bin_beg, rowname_delim,
                           bin_end);

        for (const auto col_idx : std::views::iota(0u, n_levels))
          level_format::push_elem(cursor, line_end, error,
                                  levels[ne_bin_idx, col_idx], mode, delim);

        if (write_n_cpgs)
          level_format::push(cursor, line_end, error, delim, n_cpgs[bin_idx]);
        level_format::push(cursor, line_end, error, newline);
        out.write(line.data(), std::distance(line.data(), cursor));
        ++ne_bin_idx;
      }
      else if (write_empty) {
        const auto bin_end = std::min(bin_beg + bin_size, chrom_size);
        auto cursor = line_beg;
        level_format::push(cursor, line_end, error, bin_beg, rowname_delim,
                           bin_end);
        std::memcpy(cursor, empty_row.data(), empty_row_size);
        cursor += empty_row_size;
        if (write_n_cpgs)
          level_format::push(cursor, line_end, error, delim, 0u);
        level_format::push(cursor, line_end, error, newline);
        out.write(line.data(), std::distance(line.data(), cursor));
      }
      ++bin_idx;
    }
  }
  return std::make_error_code(error);
}

template <typename level_element>
[[nodiscard]] static inline auto
write_bins_dfscores_impl(
  // clang-format off
  const std::string &outfile,
  const std::vector<std::string> &names,
  const genome_index &index,
  const std::uint32_t bin_size,
  const std::uint32_t min_reads,
  const level_container<level_element> &levels,
  const char rowname_delim,
  const bool write_header,
  const bool write_n_cpgs,
  const bool write_empty,
  const std::vector<std::uint32_t> &n_cpgs
  // clang-format on
  ) -> std::error_code {
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto delim{'\t'};
  static constexpr auto newline{'\n'};
  static constexpr auto none_label = "NA"sv;

  const auto n_levels = levels.n_cols;
  const auto empty_row = get_empty_row_scores<level_element>(
    write_empty, n_levels, delim, none_label);
  const auto empty_row_size = std::size(empty_row);

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  if (write_header) {
    const auto hdr_fmtr = [&](const auto &r) {
      return std::format(level_element::hdr_fmt_scr, r, delim, r);
    };
    auto joined = join_with(names | std::views::transform(hdr_fmtr), delim);
    if (write_n_cpgs)
      joined += std::format("{}{}", delim, "N_CPG");
    std::println(out, "{}", joined);
  }

  std::vector<char> line(bins_writer::output_buffer_size);
  // NOLINTNEXTLINE (*-pointer-arithmetic)
  const auto line_end = line.data() + std::size(line);
  auto error = std::errc{};

  std::uint32_t bin_idx = 0;     // index for all bins
  std::uint32_t ne_bin_idx = 0;  // index for bins that can be non-empty

  const auto &meta = index.get_metadata();
  const auto zipped = std::views::zip(meta.chrom_size, meta.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    auto line_beg = line.data();
    level_format::push(line_beg, line_end, error, chrom_name, rowname_delim);
    for (std::uint32_t bin_beg = 0; bin_beg < chrom_size; bin_beg += bin_size) {
      if (n_cpgs[bin_idx] > 0) {
        auto cursor = line_beg;
        level_format::push(cursor, line_end, error, bin_beg);
        for (const auto col_idx : std::views::iota(0u, n_levels))
          level_format::push_score(cursor, line_end, error,
                                   levels[ne_bin_idx, col_idx], none_label,
                                   min_reads, delim);
        if (write_n_cpgs)
          level_format::push(cursor, line_end, error, delim, n_cpgs[bin_idx]);
        level_format::push(cursor, line_end, error, newline);
        out.write(line.data(), std::distance(line.data(), cursor));
        ++ne_bin_idx;
      }
      else if (write_empty) {
        auto cursor = line_beg;
        level_format::push(cursor, line_end, error, bin_beg);
        std::memcpy(cursor, empty_row.data(), empty_row_size);
        cursor += empty_row_size;
        if (write_n_cpgs)
          level_format::push(cursor, line_end, error, delim, 0u);
        level_format::push(cursor, line_end, error, newline);
        out.write(line.data(), std::distance(line.data(), cursor));
      }
      ++bin_idx;
    }
  }
  return std::make_error_code(error);
}

template <typename level_element>
[[nodiscard]] static inline auto
write_bins_dataframe_impl(
  // clang-format off
  const std::string &outfile,
  const std::vector<std::string> &names,
  const genome_index &index,
  const std::uint32_t bin_size,
  const level_container<level_element> &levels,
  const level_element_mode mode,
  const char rowname_delim,
  const bool write_header,
  const bool write_n_cpgs,
  const bool write_empty,
  const std::vector<std::uint32_t> &n_cpgs
  // clang-format on
  ) -> std::error_code {
  static constexpr auto delim{'\t'};
  static constexpr auto newline{'\n'};

  const auto n_levels = levels.n_cols;
  const auto empty_row =
    get_empty_row<level_element>(write_empty, n_levels, delim, mode);
  const auto empty_row_size = std::size(empty_row);

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  if (write_header) {
    const auto hdr_fmtr = [&](const auto &r) {
      return mode == level_element_mode::classic
               ? std::format(level_element::hdr_fmt_cls, r, delim, r, delim, r)
               : std::format(level_element::hdr_fmt, r, delim, r, delim, r);
    };
    auto joined = join_with(names | std::views::transform(hdr_fmtr), delim);
    if (write_n_cpgs)
      joined += std::format("{}{}", delim, "N_CPG");
    std::println(out, "{}", joined);
  }

  std::vector<char> line(bins_writer::output_buffer_size);
  // NOLINTNEXTLINE (*-pointer-arithmetic)
  const auto line_end = line.data() + std::size(line);
  auto error = std::errc{};

  std::uint32_t bin_idx = 0;
  std::uint32_t ne_bin_idx = 0;  // index for bins that can be non-empty

  const auto &meta = index.get_metadata();
  const auto zipped = std::views::zip(meta.chrom_size, meta.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    auto line_beg = line.data();
    level_format::push(line_beg, line_end, error, chrom_name, rowname_delim);
    for (std::uint32_t bin_beg = 0; bin_beg < chrom_size; bin_beg += bin_size) {
      auto cursor = line_beg;
      if (n_cpgs[bin_idx] > 0) {
        level_format::push(cursor, line_end, error, bin_beg);
        for (const auto col_idx : std::views::iota(0u, n_levels))
          level_format::push_elem(cursor, line_end, error,
                                  levels[ne_bin_idx, col_idx], mode, delim);
        if (write_n_cpgs)
          level_format::push(cursor, line_end, error, delim, n_cpgs[bin_idx]);
        level_format::push(cursor, line_end, error, newline);
        out.write(line.data(), std::distance(line.data(), cursor));
        ++ne_bin_idx;  // increment the index of rows/cpgs with data
      }
      else if (write_empty) {
        level_format::push(cursor, line_end, error, bin_beg);
        std::memcpy(cursor, empty_row.data(), empty_row_size);
        cursor += empty_row_size;
        if (write_n_cpgs)
          level_format::push(cursor, line_end, error, delim, 0u);
        level_format::push(cursor, line_end, error, newline);
        out.write(line.data(), std::distance(line.data(), cursor));
      }
      ++bin_idx;  // increment the general index of rows/cpgs
    }
  }

  return std::make_error_code(error);
}

// ADS: Pseudo-declarations below

template <>
[[nodiscard]] auto
bins_writer::write_bedlike_impl(
  const std::vector<level_container_flat<level_element_t>> &levels,
  const level_element_mode mode) const noexcept -> std::error_code {
  return write_bedlike_bins_impl(outfile, index, bin_size, levels, mode,
                                 write_n_cpgs, write_empty, n_cpgs);
}

template <>
[[nodiscard]] auto
bins_writer::write_bedlike_impl(
  const std::vector<level_container_flat<level_element_covered_t>> &levels,
  const level_element_mode mode) const noexcept -> std::error_code {
  return write_bedlike_bins_impl(outfile, index, bin_size, levels, mode,
                                 write_n_cpgs, write_empty, n_cpgs);
}

template <>
[[nodiscard]] auto
bins_writer::write_bedlike_impl(const level_container<level_element_t> &levels,
                                const level_element_mode mode) const noexcept
  -> std::error_code {
  return write_bedlike_bins_impl(outfile, index, bin_size, levels, mode,
                                 write_n_cpgs, write_empty, n_cpgs);
}

template <>
[[nodiscard]] auto
bins_writer::write_bedlike_impl(
  const level_container<level_element_covered_t> &levels,
  const level_element_mode mode) const noexcept -> std::error_code {
  return write_bedlike_bins_impl(outfile, index, bin_size, levels, mode,
                                 write_n_cpgs, write_empty, n_cpgs);
}

template <>
[[nodiscard]] auto
bins_writer::write_dfscores_impl(
  const std::vector<level_container_flat<level_element_t>> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_bins_dfscores_impl(outfile, names, index, bin_size, min_reads,
                                  levels, rowname_delim, write_header,
                                  write_n_cpgs, write_empty, n_cpgs);
}

template <>
[[nodiscard]] auto
bins_writer::write_dfscores_impl(
  const std::vector<level_container_flat<level_element_covered_t>> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_bins_dfscores_impl(outfile, names, index, bin_size, min_reads,
                                  levels, rowname_delim, write_header,
                                  write_n_cpgs, write_empty, n_cpgs);
}

template <>
[[nodiscard]] auto
bins_writer::write_dfscores_impl(
  const level_container<level_element_t> &levels, const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_bins_dfscores_impl(outfile, names, index, bin_size, min_reads,
                                  levels, rowname_delim, write_header,
                                  write_n_cpgs, write_empty, n_cpgs);
}

template <>
[[nodiscard]] auto
bins_writer::write_dfscores_impl(
  const level_container<level_element_covered_t> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_bins_dfscores_impl(outfile, names, index, bin_size, min_reads,
                                  levels, rowname_delim, write_header,
                                  write_n_cpgs, write_empty, n_cpgs);
}

template <>
[[nodiscard]] auto
bins_writer::write_dataframe_impl(
  const std::vector<level_container_flat<level_element_t>> &levels,
  const level_element_mode mode, const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_bins_dataframe_impl(outfile, names, index, bin_size, levels,
                                   mode, rowname_delim, write_header,
                                   write_n_cpgs, write_empty, n_cpgs);
}

template <>
[[nodiscard]] auto
bins_writer::write_dataframe_impl(
  const std::vector<level_container_flat<level_element_covered_t>> &levels,
  const level_element_mode mode, const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_bins_dataframe_impl(outfile, names, index, bin_size, levels,
                                   mode, rowname_delim, write_header,
                                   write_n_cpgs, write_empty, n_cpgs);
}

template <>
[[nodiscard]] auto
bins_writer::write_dataframe_impl(
  const level_container<level_element_t> &levels, const level_element_mode mode,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_bins_dataframe_impl(outfile, names, index, bin_size, levels,
                                   mode, rowname_delim, write_header,
                                   write_n_cpgs, write_empty, n_cpgs);
}

template <>
[[nodiscard]] auto
bins_writer::write_dataframe_impl(
  const level_container<level_element_covered_t> &levels,
  const level_element_mode mode, const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_bins_dataframe_impl(outfile, names, index, bin_size, levels,
                                   mode, rowname_delim, write_header,
                                   write_n_cpgs, write_empty, n_cpgs);
}

}  // namespace transferase
