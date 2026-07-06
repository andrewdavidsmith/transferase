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

#include "windows_writer.hpp"

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

// ADS: first functions that accept 2D vector

[[nodiscard]] static inline auto
write_bedlike_windows_impl(const std::string &outfile,
                           const genome_index_metadata &meta,
                           const std::uint32_t window_size,
                           const std::uint32_t window_step,
                           const std::vector<std::uint32_t> &n_cpgs,
                           const auto &levels,
                           const level_element_mode mode) -> std::error_code {
  static constexpr auto delim{'\t'};
  const auto lvl_to_string = [mode](const auto &l) {
    return mode == level_element_mode::classic ? l.tostring_classic()
                                               : l.tostring_counts();
  };

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto write_n_cpgs = !n_cpgs.empty();

  const auto n_levels = std::size(levels);
  std::uint32_t i = 0;
  const auto zipped = std::views::zip(meta.chrom_size, meta.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    for (std::uint32_t window_beg = 0; window_beg < chrom_size;
         window_beg += window_step) {
      const auto window_end = std::min(window_beg + window_size, chrom_size);
      std::print(out, "{}{}{}{}{}", chrom_name, delim, window_beg, delim,
                 window_end);
      for (const auto j : std::views::iota(0u, n_levels))
        std::print(out, "{}{}", delim, lvl_to_string(levels[j][i]));
      if (write_n_cpgs)
        std::print(out, "{}{}", delim, n_cpgs[i]);
      /*println() without arg doesn't work on macos clang*/
      // std::println(out);
      std::print(out, "\n");
      ++i;
    }
  }
  return {};
}

[[nodiscard]] static inline auto
write_windows_dfscores_impl(
  // clang-format off
  const std::string &outfile,
  const std::vector<std::string> &names,
  const genome_index_metadata &meta,
  const std::uint32_t window_size,
  const std::uint32_t window_step,
  const std::uint32_t min_reads,
  const std::vector<std::uint32_t> &n_cpgs,
  const auto &levels, const char rowname_delim,
  const bool write_header
  // clang-format on
  ) -> std::error_code {
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto none_label = "NA"sv;
  static constexpr auto delim{'\t'};

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto write_n_cpgs = !n_cpgs.empty();

  if (write_header) {
    // auto joined =
    //   names | std::views::join_with(delim) | std::ranges::to<std::string>();
    auto joined = join_with(names, delim);
    if (write_n_cpgs)
      joined += std::format("{}{}", delim, "N_CPG"sv);
    std::println(out, "{}", joined);
  }

  const auto n_levels = std::size(levels);
  std::uint32_t i = 0;
  const auto zipped = std::views::zip(meta.chrom_size, meta.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    for (std::uint32_t window_beg = 0; window_beg < chrom_size;
         window_beg += window_step) {
      const auto window_end = std::min(window_beg + window_size, chrom_size);
      std::print(out, "{}{}{}{}{}", chrom_name, rowname_delim, window_beg,
                 rowname_delim, window_end);
      for (const auto j : std::views::iota(0u, n_levels))
        if (levels[j][i].n_reads() >= min_reads)
          std::print(out, "{}{:.6}", delim, levels[j][i].get_wmean());
        else
          std::print(out, "{}{}", delim, none_label);
      /*println() without arg doesn't work on macos clang*/
      // std::println(out);
      std::print(out, "\n");
      ++i;
    }
  }
  return {};
}

[[nodiscard]] static inline auto
write_windows_dataframe_impl(
  // clang-format off
  const std::string &outfile,
  const std::vector<std::string> &names,
  const genome_index_metadata &meta,
  const std::uint32_t window_size,
  const std::uint32_t window_step,
  const std::vector<std::uint32_t> &n_cpgs,
  const auto &levels, const level_element_mode mode,
  const char rowname_delim,
  const bool write_header
  // clang-format on
  ) -> std::error_code {
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto delim = '\t';  // not optional for now
  // determine type
  using outer_type = typename std::remove_cvref_t<decltype(levels)>::value_type;
  using level_element = typename std::remove_cvref_t<outer_type>::value_type;
  const auto hdr_formatter = [&](const auto &r) {
    return mode == level_element_mode::classic
             ? std::format(level_element::hdr_fmt_cls, r, delim, r, delim, r)
             : std::format(level_element::hdr_fmt, r, delim, r, delim, r);
  };
  const auto lvl_to_string = [mode](const auto &l) {
    return mode == level_element_mode::classic ? l.tostring_classic()
                                               : l.tostring_counts();
  };

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto write_n_cpgs = !n_cpgs.empty();

  if (write_header) {
    // auto joined = names | std::views::transform(hdr_formatter) |
    //               std::views::join_with(delim) |
    //               std::ranges::to<std::string>();
    auto joined =
      join_with(names | std::views::transform(hdr_formatter), delim);
    if (write_n_cpgs)
      joined += std::format("{}{}", delim, "N_CPG"sv);
    std::println(out, "{}", joined);
  }

  const auto n_levels = std::size(levels);
  std::uint32_t i = 0;
  const auto zipped = std::views::zip(meta.chrom_size, meta.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped)
    for (std::uint32_t window_beg = 0; window_beg < chrom_size;
         window_beg += window_step) {
      const auto window_end = std::min(window_beg + window_size, chrom_size);
      std::print(out, "{}{}{}{}{}", chrom_name, rowname_delim, window_beg,
                 rowname_delim, window_end);
      for (const auto j : std::views::iota(0u, n_levels))
        std::print(out, "{}{}", delim, lvl_to_string(levels[j][i]));
      if (write_n_cpgs)
        std::print(out, "{}{}", delim, n_cpgs[i]);
      /*println() without arg doesn't work on macos clang*/
      // std::println(out);
      std::print(out, "\n");
      ++i;
    }

  return {};
}

// ADS: now functions that accept level_container

template <typename level_element>
[[nodiscard]] static inline auto
write_bedlike_windows_impl(
  // clang-format off
  const std::string &outfile,
  const genome_index_metadata &meta,
  const std::uint32_t window_size,
  const std::uint32_t window_step,
  const std::vector<std::uint32_t> &n_cpgs,
  const level_container<level_element> &levels,
  const level_element_mode mode
  // clang-format on
  ) -> std::error_code {
  static constexpr auto rowname_delim{'\t'};
  static constexpr auto delim{'\t'};
  static constexpr auto newline{'\n'};

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto write_n_cpgs = !n_cpgs.empty();

  std::vector<char> line(windows_writer::output_buffer_size);
  // NOLINTNEXTLINE (*-pointer-arithmetic)
  const auto line_end = std::data(line) + std::size(line);
  auto error = std::errc{};

  const auto n_levels = levels.n_cols;
  std::uint32_t i = 0;

  const auto zipped = std::views::zip(meta.chrom_size, meta.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    auto line_beg = std::data(line);
    push_buffer(line_beg, line_end, error, chrom_name, rowname_delim);

    for (std::uint32_t window_beg = 0; window_beg < chrom_size;
         window_beg += window_step) {
      const auto window_end = std::min(window_beg + window_size, chrom_size);
      auto cursor = line_beg;
      push_buffer(cursor, line_end, error, window_beg, rowname_delim,
                  window_end);

      for (const auto j : std::views::iota(0u, n_levels))
        push_buffer_elem(cursor, line_end, error, levels[i, j], mode, delim);

      if (write_n_cpgs)
        push_buffer(cursor, line_end, error, delim, n_cpgs[i]);
      push_buffer(cursor, line_end, error, newline);
      out.write(std::data(line), std::distance(std::data(line), cursor));
      ++i;
    }
  }
  return std::make_error_code(error);
}

template <typename level_element>
[[nodiscard]] static inline auto
write_windows_dfscores_impl(
  // clang-format off
  const std::string &outfile,
  const std::vector<std::string> &names,
  const genome_index_metadata &meta,
  const std::uint32_t window_size,
  const std::uint32_t window_step,
  const std::uint32_t min_reads,
  const std::vector<std::uint32_t> &n_cpgs,
  const level_container<level_element> &levels,
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
    // auto joined =
    //   names | std::views::join_with(delim) | std::ranges::to<std::string>();
    auto joined = join_with(names, delim);
    if (write_n_cpgs)
      joined += std::format("{}{}", delim, "N_CPG");
    std::println(out, "{}", joined);
  }

  std::vector<char> line(windows_writer::output_buffer_size);
  // NOLINTNEXTLINE (*-pointer-arithmetic)
  const auto line_end = std::data(line) + std::size(line);
  auto error = std::errc{};

  const auto n_levels = levels.n_cols;
  std::uint32_t i = 0;

  const auto zipped = std::views::zip(meta.chrom_size, meta.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    auto line_beg = std::data(line);
    push_buffer(line_beg, line_end, error, chrom_name, rowname_delim);
    for (std::uint32_t window_beg = 0; window_beg < chrom_size;
         window_beg += window_step) {
      const auto window_end = std::min(window_beg + window_size, chrom_size);
      auto cursor = line_beg;
      push_buffer(cursor, line_end, error, window_beg, rowname_delim,
                  window_end);
      for (const auto j : std::views::iota(0u, n_levels))
        push_buffer_score(cursor, line_end, error, levels[i, j], none_label,
                          min_reads, delim);
      if (write_n_cpgs)
        push_buffer(cursor, line_end, error, delim, n_cpgs[i]);
      push_buffer(cursor, line_end, error, newline);
      out.write(std::data(line), std::distance(std::data(line), cursor));
      ++i;
    }
  }
  return std::make_error_code(error);
}

template <typename level_element>
[[nodiscard]] static inline auto
write_windows_dataframe_impl(
  // clang-format off
  const std::string &outfile,
  const std::vector<std::string> &names,
  const genome_index_metadata &meta,
  const std::uint32_t window_size,
  const std::uint32_t window_step,
  const std::vector<std::uint32_t> &n_cpgs,
  const level_container<level_element> &levels,
  const level_element_mode mode,
  const char rowname_delim,
  const bool write_header
  // clang-format on
  ) -> std::error_code {
  static constexpr auto delim{'\t'};
  static constexpr auto newline{'\n'};

  const auto hdr_formatter = [&](const auto &r) {
    return mode == level_element_mode::classic
             ? std::format(level_element::hdr_fmt_cls, r, delim, r, delim, r)
             : std::format(level_element::hdr_fmt, r, delim, r, delim, r);
  };

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto write_n_cpgs = !n_cpgs.empty();

  if (write_header) {
    // auto joined = names | std::views::transform(hdr_formatter) |
    //               std::views::join_with(delim) |
    //               std::ranges::to<std::string>();
    auto joined =
      join_with(names | std::views::transform(hdr_formatter), delim);
    if (write_n_cpgs)
      joined += std::format("{}{}", delim, "N_CPG");
    std::println(out, "{}", joined);
  }

  std::vector<char> line(windows_writer::output_buffer_size);
  // NOLINTNEXTLINE (*-pointer-arithmetic)
  const auto line_end = std::data(line) + std::size(line);
  auto error = std::errc{};

  const auto n_levels = levels.n_cols;
  std::uint32_t i = 0;

  const auto zipped = std::views::zip(meta.chrom_size, meta.chrom_order);
  for (const auto [chrom_size, chrom_name] : zipped) {
    auto line_beg = std::data(line);
    push_buffer(line_beg, line_end, error, chrom_name, rowname_delim);
    for (std::uint32_t window_beg = 0; window_beg < chrom_size;
         window_beg += window_step) {
      const auto window_end = std::min(window_beg + window_size, chrom_size);
      auto cursor = line_beg;
      push_buffer(cursor, line_end, error, window_beg, rowname_delim,
                  window_end);
      for (const auto j : std::views::iota(0u, n_levels))
        push_buffer_elem(cursor, line_end, error, levels[i, j], mode, delim);
      if (write_n_cpgs)
        push_buffer(cursor, line_end, error, delim, n_cpgs[i]);
      push_buffer(cursor, line_end, error, newline);
      out.write(std::data(line), std::distance(std::data(line), cursor));
      ++i;
    }
  }
  return std::make_error_code(error);
}

// ADS: definitions for specializations below

template <>
[[nodiscard]] auto
windows_writer::write_bedlike_impl(
  const std::vector<level_container_flat<level_element_t>> &levels,
  const level_element_mode mode) const noexcept -> std::error_code {
  return write_bedlike_windows_impl(outfile, index.get_metadata(), window_size,
                                    window_step, n_cpgs, levels, mode);
}

template <>
[[nodiscard]] auto
windows_writer::write_bedlike_impl(
  const std::vector<level_container_flat<level_element_covered_t>> &levels,
  const level_element_mode mode) const noexcept -> std::error_code {
  return write_bedlike_windows_impl(outfile, index.get_metadata(), window_size,
                                    window_step, n_cpgs, levels, mode);
}

template <>
[[nodiscard]] auto
windows_writer::write_bedlike_impl(
  const level_container<level_element_t> &levels,
  const level_element_mode mode) const noexcept -> std::error_code {
  return write_bedlike_windows_impl(outfile, index.get_metadata(), window_size,
                                    window_step, n_cpgs, levels, mode);
}

template <>
[[nodiscard]] auto
windows_writer::write_bedlike_impl(
  const level_container<level_element_covered_t> &levels,
  const level_element_mode mode) const noexcept -> std::error_code {
  return write_bedlike_windows_impl(outfile, index.get_metadata(), window_size,
                                    window_step, n_cpgs, levels, mode);
}

template <>
[[nodiscard]] auto
windows_writer::write_dfscores_impl(
  const std::vector<level_container_flat<level_element_t>> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_windows_dfscores_impl(
    outfile, names, index.get_metadata(), window_size, window_step, min_reads,
    n_cpgs, levels, rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
windows_writer::write_dfscores_impl(
  const std::vector<level_container_flat<level_element_covered_t>> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_windows_dfscores_impl(
    outfile, names, index.get_metadata(), window_size, window_step, min_reads,
    n_cpgs, levels, rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
windows_writer::write_dfscores_impl(
  const level_container<level_element_t> &levels, const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_windows_dfscores_impl(
    outfile, names, index.get_metadata(), window_size, window_step, min_reads,
    n_cpgs, levels, rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
windows_writer::write_dfscores_impl(
  const level_container<level_element_covered_t> &levels,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_windows_dfscores_impl(
    outfile, names, index.get_metadata(), window_size, window_step, min_reads,
    n_cpgs, levels, rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
windows_writer::write_dataframe_impl(
  const std::vector<level_container_flat<level_element_t>> &levels,
  const level_element_mode mode, const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_windows_dataframe_impl(outfile, names, index.get_metadata(),
                                      window_size, window_step, n_cpgs, levels,
                                      mode, rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
windows_writer::write_dataframe_impl(
  const std::vector<level_container_flat<level_element_covered_t>> &levels,
  const level_element_mode mode, const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_windows_dataframe_impl(outfile, names, index.get_metadata(),
                                      window_size, window_step, n_cpgs, levels,
                                      mode, rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
windows_writer::write_dataframe_impl(
  const level_container<level_element_t> &levels, const level_element_mode mode,
  const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_windows_dataframe_impl(outfile, names, index.get_metadata(),
                                      window_size, window_step, n_cpgs, levels,
                                      mode, rowname_delim, write_header);
}

template <>
[[nodiscard]] auto
windows_writer::write_dataframe_impl(
  const level_container<level_element_covered_t> &levels,
  const level_element_mode mode, const char rowname_delim,
  const bool write_header) const noexcept -> std::error_code {
  return write_windows_dataframe_impl(outfile, names, index.get_metadata(),
                                      window_size, window_step, n_cpgs, levels,
                                      mode, rowname_delim, write_header);
}

}  // namespace transferase
