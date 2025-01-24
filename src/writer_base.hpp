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

#ifndef SRC_WRITER_BASE_HPP_
#define SRC_WRITER_BASE_HPP_

#include "output_format_type.hpp"

#include <string>
#include <string_view>
#include <system_error>
#include <variant>  // for std::visit
#include <vector>

namespace transferase {

struct genomic_interval;
struct genome_index;
struct level_element_t;
struct level_element_covered_t;

template <typename T>
concept LevelsInputRangeSingle =
  std::ranges::input_range<T> &&
  (std::same_as<std::ranges::range_value_t<T>, level_element_t> ||
   std::same_as<std::ranges::range_value_t<T>, level_element_covered_t>);

static_assert(LevelsInputRangeSingle<std::vector<level_element_t>>);
static_assert(LevelsInputRangeSingle<std::vector<level_element_t> &>);
static_assert(LevelsInputRangeSingle<std::vector<level_element_covered_t>>);
static_assert(LevelsInputRangeSingle<std::vector<level_element_covered_t> &>);

template <typename T> struct writer_base {
  const std::string &outfile;
  const genome_index &index;
  const output_format_t out_fmt;
  const std::vector<std::string> &names;
  writer_base(const std::string &outfile, const genome_index &index,
              const output_format_t &out_fmt,
              const std::vector<std::string> &names) :
    outfile{outfile}, index{index}, out_fmt{out_fmt}, names{names} {}

  // clang-format off
  auto self() noexcept -> T & {return static_cast<T &>(*this);}
  auto self() const noexcept -> const T & {return static_cast<const T &>(*this);}
  // clang-format on

  [[nodiscard]] auto
  write(const auto &levels) const noexcept -> std::error_code {
    return self().write_impl(levels);
  }

  [[nodiscard]] auto
  write_bedgraph(const auto &levels) const noexcept -> std::error_code {
    return self().write_bedgraph_impl(levels);
  }

  [[nodiscard]] auto
  write_dataframe(const auto &levels) const noexcept -> std::error_code {
    return self().write_dataframe_impl(levels);
  }

  [[nodiscard]] auto
  write_dataframe_scores(const auto &levels) const noexcept -> std::error_code {
    return self().write_dataframe_scores_impl(levels);
  }

  [[nodiscard]] auto
  write_output(const auto &levels) const noexcept -> std::error_code {
    switch (out_fmt) {
    case output_format_t::none:
      return write(levels);
    case output_format_t::counts:
      return write(levels);
    case output_format_t::bedgraph:
      return write_bedgraph(levels);
    case output_format_t::dataframe:
      return write_dataframe(levels);
    case output_format_t::dataframe_scores:
      return write_dataframe_scores(levels);
    }
    std::unreachable();
  }
};

}  // namespace transferase

#endif  // SRC_WRITER_BASE_HPP_
