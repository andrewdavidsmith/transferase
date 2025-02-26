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
#include "level_element.hpp"

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
write_intervals_impl(
  const std::string &outfile, const genome_index_metadata &meta,
  const std::vector<genomic_interval> &intervals,
  const LevelsInputRangeSingle auto &levels) noexcept -> std::error_code {
  static constexpr auto buf_size{512};
  // ADS: modified_buf_size is to ensure we can't go past the end of
  // the actual buffer with the (*tcr.ptr++) below
  static constexpr auto modified_buf_size{buf_size - 5};
  static constexpr auto delim{'\t'};

  std::array<char, buf_size> buf{};
  const auto buf_end = buf.data() + modified_buf_size;

  using level_element =
    typename std::remove_cvref_t<decltype(levels)>::value_type;
  using intervals_level = std::pair<const genomic_interval &, level_element>;
  const auto same_chrom = [](const intervals_level &a,
                             const intervals_level &b) {
    return a.first.ch_id == b.first.ch_id;
  };

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-*)
  for (const auto &chunk :
       std::views::zip(intervals, levels) | std::views::chunk_by(same_chrom)) {
    const auto ch_id = get<0>(chunk.front()).ch_id;
    const std::string chrom{meta.chrom_order[ch_id]};
    std::ranges::copy(chrom, buf.data());
    buf[std::size(chrom)] = delim;
    for (const auto &[gi, single_result] : chunk) {
      std::to_chars_result tcr{buf.data() + std::size(chrom) + 1, std::errc()};
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wstringop-overflow=0"
#endif
      tcr = std::to_chars(tcr.ptr, buf_end, gi.start);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, gi.stop);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, single_result.n_meth);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, single_result.n_unmeth);
      if constexpr (std::is_same_v<level_element, level_element_covered_t>) {
        *tcr.ptr++ = delim;
        tcr = std::to_chars(tcr.ptr, buf_end, single_result.n_covered);
      }
      *tcr.ptr++ = '\n';
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic pop
#endif
      out.write(buf.data(), std::ranges::distance(buf.data(), tcr.ptr));
      if (!out)
        return std::make_error_code(std::errc(errno));
    }
  }
  // NOLINTEND(cppcoreguidelines-pro-bounds-*)
  return {};
}

[[nodiscard]] static inline auto
write_intervals_impl(const std::string &outfile,
                     const genome_index_metadata &meta,
                     const std::vector<genomic_interval> &intervals,
                     const auto &levels) noexcept -> std::error_code {
  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  using level_container_type =
    typename std::remove_cvref_t<decltype(levels)>::value_type;
  using level_element =
    typename std::remove_cvref_t<level_container_type>::value_type;

  const auto n_levels = std::size(levels);
  auto prev_ch_id = genomic_interval::not_a_chrom;
  std::string chrom;
  for (const auto [i, interval] : std::views::enumerate(intervals)) {
    if (interval.ch_id != prev_ch_id) {
      chrom = meta.chrom_order[interval.ch_id];
      prev_ch_id = interval.ch_id;
    }
    std::print(out, "{}\t{}\t{}", chrom, interval.start, interval.stop);
    for (auto j = 0u; j < n_levels; ++j) {
      if constexpr (std::is_same_v<level_element, level_element_covered_t>)
        std::print(out, "\t{}\t{}\t{}", levels[j][i].n_meth,
                   levels[j][i].n_unmeth, levels[j][i].n_covered);
      else if constexpr (std::is_same_v<level_element, level_element_t>)
        std::print(out, "\t{}\t{}", levels[j][i].n_meth, levels[j][i].n_unmeth);
      else
        static_assert(false, "level_element has invalid type");
    }
    std::println(out);
  }
  return {};
}

[[nodiscard]] static inline auto
write_intervals_bedgraph_impl(
  const std::string &outfile, const genome_index_metadata &meta,
  const std::vector<genomic_interval> &intervals,
  std::ranges::input_range auto &&scores) noexcept -> std::error_code {
  static constexpr auto score_precision{6};
  static constexpr auto buf_size{512};
  // ADS: modified_buf_size is to ensure we can't go past the end of
  // the actual buffer with the (*tcr.ptr++) below
  static constexpr auto modified_buf_size{buf_size - 3};
  static constexpr auto delim{'\t'};

  std::array<char, buf_size> buf{};
  const auto buf_end = buf.data() + modified_buf_size;

  using intervals_score = std::pair<const genomic_interval &, const double>;
  const auto same_chrom = [](const intervals_score &a,
                             const intervals_score &b) {
    return a.first.ch_id == b.first.ch_id;
  };

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-*)
  for (const auto &chunk :
       std::views::zip(intervals, scores) | std::views::chunk_by(same_chrom)) {
    const auto ch_id = get<0>(chunk.front()).ch_id;
    const std::string chrom{meta.chrom_order[ch_id]};
    std::ranges::copy(chrom, buf.data());
    buf[std::size(chrom)] = delim;
    for (const auto &[gi, single_score] : chunk) {
      std::to_chars_result tcr{buf.data() + std::size(chrom) + 1, std::errc()};
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wstringop-overflow=0"
#endif
      tcr = std::to_chars(tcr.ptr, buf_end, gi.start);
      *tcr.ptr++ = delim;
      tcr = std::to_chars(tcr.ptr, buf_end, gi.stop);
      *tcr.ptr++ = delim;
      // ADS: below the format is intended to mimick default for cout
      tcr = std::to_chars(tcr.ptr, buf_end, single_score,
                          std::chars_format::general, score_precision);
      *tcr.ptr++ = '\n';
#if defined(__GNUG__) and not defined(__clang__)
#pragma GCC diagnostic pop
#endif
      out.write(buf.data(), std::ranges::distance(buf.data(), tcr.ptr));
      if (!out)
        return std::make_error_code(std::errc(errno));
    }
  }
  // NOLINTEND(cppcoreguidelines-pro-bounds-*)
  return {};
}

[[nodiscard]] static inline auto
write_intervals_bedgraph_impl(const std::string &outfile,
                              const genome_index_metadata &meta,
                              const std::vector<genomic_interval> &intervals,
                              const auto &levels) noexcept -> std::error_code {
  const auto get_score = [](const auto x) {
    const double total = x.n_meth + x.n_unmeth;
    return x.n_meth / std::max(1.0, total);
  };
  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));
  const auto n_levels = std::size(levels);
  auto prev_ch_id = genomic_interval::not_a_chrom;
  std::string chrom;
  for (const auto [i, interval] : std::views::enumerate(intervals)) {
    if (interval.ch_id != prev_ch_id) {
      chrom = meta.chrom_order[interval.ch_id];
      prev_ch_id = interval.ch_id;
    }
    std::print(out, "{}\t{}\t{}", chrom, interval.start, interval.stop);
    for (auto j = 0u; j < n_levels; ++j)
      std::print(out, "\t{:.6}", get_score(levels[j][i]));
    std::println(out);
  }
  return {};
}

[[nodiscard]] static inline auto
write_intervals_dataframe_scores_impl(
  const std::string &outfile, const std::vector<std::string> &names,
  const genome_index_metadata &meta,
  const std::vector<genomic_interval> &intervals, const std::uint32_t min_reads,
  const auto &levels) -> std::error_code {
  using std::literals::string_view_literals::operator""sv;
  static constexpr auto none_label = "NA"sv;
  // static constexpr auto delim = '\t';  // not optional for now

  const auto get_score = [](const auto x) {
    const double total = x.n_meth + x.n_unmeth;
    return x.n_meth / std::max(1.0, total);
  };

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  const auto joined =
    names | std::views::join_with('\t') | std::ranges::to<std::string>();
  std::println(out, "{}", joined);

  const auto n_levels = std::size(levels);
  auto prev_ch_id = genomic_interval::not_a_chrom;
  std::string chrom;
  for (const auto [i, interval] : std::views::enumerate(intervals)) {
    if (interval.ch_id != prev_ch_id) {
      chrom = meta.chrom_order[interval.ch_id];
      prev_ch_id = interval.ch_id;
    }
    std::print(out, "{}.{}.{}", chrom, interval.start, interval.stop);
    for (auto j = 0u; j < n_levels; ++j)
      if (levels[j][i].n_reads() >= min_reads)
        std::print(out, "\t{:.6}", get_score(levels[j][i]));
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

  std::ofstream out(outfile);
  if (!out)
    return std::make_error_code(std::errc(errno));

  using level_container_type =
    typename std::remove_cvref_t<decltype(levels)>::value_type;
  using level_element =
    typename std::remove_cvref_t<level_container_type>::value_type;

  if constexpr (std::is_same_v<level_element, level_element_covered_t>) {
    static constexpr auto hdr_lvl_cov_fmt = "{}_M\t{}_U\t{}_C";
    const auto hdr_formatter = [&](const auto &r) {
      return std::format(hdr_lvl_cov_fmt, r, r, r);
    };
    const auto joined = names | std::views::transform(hdr_formatter) |
                        std::views::join_with('\t') |
                        std::ranges::to<std::string>();
    std::println(out, "{}", joined);
  }
  else if constexpr (std::is_same_v<level_element, level_element_t>) {
    static constexpr auto hdr_lvl_fmt = "{}_M\t{}_U";
    const auto hdr_formatter = [&](const auto &r) {
      return std::format(hdr_lvl_fmt, r, r);
    };
    const auto joined = names | std::views::transform(hdr_formatter) |
                        std::views::join_with('\t') |
                        std::ranges::to<std::string>();
    std::println(out, "{}", joined);
  }
  else
    static_assert(false, "level_element has invalid type");

  const auto n_levels = std::size(levels);
  auto prev_ch_id = genomic_interval::not_a_chrom;
  std::string chrom;
  for (const auto [i, interval] : std::views::enumerate(intervals)) {
    if (interval.ch_id != prev_ch_id) {
      chrom = meta.chrom_order[interval.ch_id];
      prev_ch_id = interval.ch_id;
    }
    std::print(out, "{}.{}.{}", chrom, interval.start, interval.stop);
    for (auto j = 0u; j < n_levels; ++j) {
      if constexpr (std::is_same_v<level_element, level_element_covered_t>) {
        std::print(out, "\t{}\t{}\t{}", levels[j][i].n_meth,
                   levels[j][i].n_unmeth, levels[j][i].n_covered);
      }
      else if constexpr (std::is_same_v<level_element, level_element_t>) {
        std::print(out, "\t{}\t{}", levels[j][i].n_meth, levels[j][i].n_unmeth);
      }
      else
        static_assert(false, "level_element has invalid type");
    }
    std::println(out);
  }
  return {};
}

template <>
[[nodiscard]] auto
intervals_writer::write_bedgraph_impl(
  const std::vector<level_element_t> &levels) const noexcept
  -> std::error_code {
  const auto to_score = [](const auto &x) {
    return x.n_meth / std::max(1.0, static_cast<double>(x.n_meth + x.n_unmeth));
  };
  return write_intervals_bedgraph_impl(outfile, index.get_metadata(), intervals,
                                       std::views::transform(levels, to_score));
}

template <>
[[nodiscard]] auto
intervals_writer::write_bedgraph_impl(
  const std::vector<level_element_covered_t> &levels) const noexcept
  -> std::error_code {
  const auto to_score = [](const auto &x) {
    return x.n_meth / std::max(1.0, static_cast<double>(x.n_meth + x.n_unmeth));
  };
  return write_intervals_bedgraph_impl(outfile, index.get_metadata(), intervals,
                                       std::views::transform(levels, to_score));
}

template <>
[[nodiscard]] auto
intervals_writer::write_bedgraph_impl(
  const std::vector<level_container<level_element_t>> &levels) const noexcept
  -> std::error_code {
  return write_intervals_bedgraph_impl(outfile, index.get_metadata(), intervals,
                                       levels);
}

template <>
[[nodiscard]] auto
intervals_writer::write_bedgraph_impl(
  const std::vector<level_container<level_element_covered_t>> &levels)
  const noexcept -> std::error_code {
  return write_intervals_bedgraph_impl(outfile, index.get_metadata(), intervals,
                                       levels);
}

template <>
[[nodiscard]] auto
intervals_writer::write_impl(const std::vector<level_element_t> &levels)
  const noexcept -> std::error_code {
  return write_intervals_impl(outfile, index.get_metadata(), intervals, levels);
}

template <>
[[nodiscard]] auto
intervals_writer::write_impl(const std::vector<level_element_covered_t> &levels)
  const noexcept -> std::error_code {
  return write_intervals_impl(outfile, index.get_metadata(), intervals, levels);
}

template <>
[[nodiscard]] auto
intervals_writer::write_impl(
  const std::vector<level_container<level_element_covered_t>> &levels)
  const noexcept -> std::error_code {
  return write_intervals_impl(outfile, index.get_metadata(), intervals, levels);
}

template <>
[[nodiscard]] auto
intervals_writer::write_impl(const std::vector<level_container<level_element_t>>
                               &levels) const noexcept -> std::error_code {
  return write_intervals_impl(outfile, index.get_metadata(), intervals, levels);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_scores_impl(
  const std::vector<level_container<level_element_t>> &levels) const noexcept
  -> std::error_code {
  return write_intervals_dataframe_scores_impl(
    outfile, names, index.get_metadata(), intervals, min_reads, levels);
}

template <>
[[nodiscard]] auto
intervals_writer::write_dataframe_scores_impl(
  const std::vector<level_container<level_element_covered_t>> &levels)
  const noexcept -> std::error_code {
  return write_intervals_dataframe_scores_impl(
    outfile, names, index.get_metadata(), intervals, min_reads, levels);
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

}  // namespace transferase
