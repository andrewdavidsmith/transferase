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

#ifndef SRC_GENOMIC_INTERVAL_OUTPUT_HPP_
#define SRC_GENOMIC_INTERVAL_OUTPUT_HPP_

#include "genome_index.hpp"
#include "genome_index_metadata.hpp"
#include "genomic_interval.hpp"
#include "level_container.hpp"
#include "logger.hpp"
#include "output_format_type.hpp"

#include <algorithm>  // std::min
#include <array>
#include <cassert>
#include <cerrno>
#include <charconv>
#include <cstdint>  // for std::uint32_t
#include <fstream>
#include <iterator>  // for std::size, std::cbegin, std::cend
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>  // for std::is_same_v
#include <utility>      // for std::pair
#include <variant>
#include <vector>

namespace transferase {

struct level_element_covered_t;
struct level_element_t;

// template <typename T> struct output_mgr {
//   const std::string &outfile;
//   const genome_index &index;
//   const output_format_t out_fmt;
//   const std::vector<std::string> &names;
//   output_mgr(const std::string &outfile, const genome_index &index,
//              const output_format_t &out_fmt,
//              const std::vector<std::string> &names) :
//     outfile{outfile}, index{index}, out_fmt{out_fmt}, names{names} {}

//   auto
//   self() noexcept -> T & {
//     return static_cast<T &>(*this);
//   }
//   auto
//   self() const noexcept -> const T & {
//     return static_cast<const T &>(*this);
//   }

//   [[nodiscard]] auto
//   write_output(const auto &levels) const noexcept -> std::error_code {
//     switch (out_fmt) {
//     case output_format_t::none:
//       return self().write(levels);
//       break;
//     case output_format_t::counts:
//       return self().write(levels);
//       break;
//     case output_format_t::bedgraph:
//       return self().write_bedgraph(levels);
//       break;
//     case output_format_t::dataframe:
//       return self().write_dataframe(levels);
//       break;
//     }
//     std::unreachable();
//   }
// };

// struct intervals_output_mgr : public output_mgr<intervals_output_mgr> {
struct intervals_output_mgr {
  const std::string &outfile;
  const genome_index &index;
  const output_format_t out_fmt;
  const std::vector<std::string> &names;
  const std::vector<genomic_interval> &intervals;
  intervals_output_mgr(const std::string &outfile, const genome_index &index,
                       const output_format_t &out_fmt,
                       const std::vector<std::string> &names,
                       const std::vector<genomic_interval> &intervals) :
    outfile{outfile}, index{index}, out_fmt{out_fmt}, names{names},
    intervals{intervals} {}
  //    output_mgr{outfile, index, out_fmt, names}, intervals{intervals} {}

  [[nodiscard]] auto
  write(std::ranges::input_range auto &&levels) const noexcept
    -> std::error_code;

  [[nodiscard]] auto
  write(const auto &levels) const noexcept -> std::error_code;

  [[nodiscard]] auto
  write_bedgraph(const std::vector<double> &scores) const noexcept
    -> std::error_code;

  [[nodiscard]] auto
  write_bedgraph(const auto &levels) const noexcept -> std::error_code;

  [[nodiscard]] auto
  write_dataframe(const auto &levels) const noexcept -> std::error_code;

  [[nodiscard]] auto
  write_output(const auto &levels) const noexcept -> std::error_code {
    switch (out_fmt) {
    case output_format_t::none:
      return write(levels);
      break;
    case output_format_t::counts:
      return write(levels);
      break;
    case output_format_t::bedgraph:
      return write_bedgraph(levels);
      break;
    case output_format_t::dataframe:
      return write_dataframe(levels);
      break;
    }
    std::unreachable();
  }
};

// struct bins_output_mgr : public output_mgr<bins_output_mgr> {
struct bins_output_mgr {
  const std::string &outfile;
  const genome_index &index;
  const output_format_t out_fmt;
  const std::vector<std::string> &names;
  const std::uint32_t bin_size;
  bins_output_mgr(const std::string &outfile, const genome_index &index,
                  const output_format_t &out_fmt,
                  const std::vector<std::string> &names,
                  const std::uint32_t &bin_size) :
    outfile{outfile}, index{index}, out_fmt{out_fmt}, names{names},
    bin_size{bin_size} {}
  // output_mgr{outfile, index, out_fmt, names}, bin_size{bin_size} {}

  [[nodiscard]] auto
  write(std::ranges::input_range auto &&levels) const noexcept
    -> std::error_code;

  [[nodiscard]] auto
  write(const auto &levels) const noexcept -> std::error_code;

  [[nodiscard]] auto
  write_bedgraph(const std::vector<double> &scores) const noexcept
    -> std::error_code;

  [[nodiscard]] auto
  write_bedgraph(const auto &levels) const noexcept -> std::error_code;

  [[nodiscard]] auto
  write_dataframe(const auto &levels) const noexcept -> std::error_code;

  [[nodiscard]] auto
  write_output(const auto &levels) const noexcept -> std::error_code {
    switch (out_fmt) {
    case output_format_t::none:
      return write(levels);
      break;
    case output_format_t::counts:
      return write(levels);
      break;
    case output_format_t::bedgraph:
      return write_bedgraph(levels);
      break;
    case output_format_t::dataframe:
      return write_dataframe(levels);
      break;
    }
    std::unreachable();
  }
};

template <typename T>
concept LevelsInputRange =
  std::ranges::input_range<T> &&
  (std::same_as<std::ranges::range_value_t<T>, level_element_t> ||
   std::same_as<std::ranges::range_value_t<T>, level_element_covered_t>);

static_assert(LevelsInputRange<std::vector<level_element_t>>);
static_assert(LevelsInputRange<std::vector<level_element_t> &>);
static_assert(LevelsInputRange<std::vector<level_element_covered_t>>);
static_assert(LevelsInputRange<std::vector<level_element_covered_t> &>);

[[nodiscard]] inline auto
write_output(const intervals_output_mgr &m,
             const LevelsInputRange auto &levels) {
  auto &lgr = logger::instance();
  if (m.out_fmt == output_format_t::counts)
    return write_intervals(m.outfile, m.index.meta, m.intervals, levels);
  // ADS: counting intervals that have no reads
  std::uint32_t zero_coverage = 0;
  const auto get_score = [&zero_coverage](const auto &x) {
    zero_coverage += (x.n_meth + x.n_unmeth == 0);
    return x.n_meth / std::max(1.0, static_cast<double>(x.n_meth + x.n_unmeth));
  };
  lgr.debug("Number of intervals without reads: {}", zero_coverage);
  return m.write_bedgraph(std::views::transform(levels, get_score));
}

[[nodiscard]] inline auto
write_output(const bins_output_mgr &m, const LevelsInputRange auto &levels) {
  auto &lgr = logger::instance();
  if (m.out_fmt == output_format_t::counts)
    return write_bins(m.outfile, m.index.meta, m.bin_size, levels);
  // ADS: counting intervals that have no reads
  std::uint32_t zero_coverage = 0;
  const auto get_score = [&zero_coverage](const auto &x) {
    zero_coverage += (x.n_meth + x.n_unmeth == 0);
    return x.n_meth / std::max(1.0, static_cast<double>(x.n_meth + x.n_unmeth));
  };
  lgr.debug("Number of bins without reads: {}", zero_coverage);
  const auto scores = std::views::transform(levels, get_score);
  return m.write_bedgraph(scores);
}

[[nodiscard]] inline auto
write_output(const auto &outmgr, const auto &results) -> std::error_code {
  std::error_code ec;
  std::visit([&](const auto &arg) { ec = outmgr.write_output(arg); }, results);
  return ec;
}

}  // namespace transferase

template <>
struct std::formatter<transferase::output_format_t>
  : std::formatter<std::string> {
  auto
  format(const transferase::output_format_t &of,
         std::format_context &ctx) const {
    return std::format_to(
      ctx.out(), "{}",
      transferase::output_format_t_name[std::to_underlying(of)]);
  }
};

#endif  // SRC_GENOMIC_INTERVAL_OUTPUT_HPP_
