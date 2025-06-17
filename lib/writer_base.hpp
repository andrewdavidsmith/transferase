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

#ifndef LIB_WRITER_BASE_HPP_
#define LIB_WRITER_BASE_HPP_

#include "level_element_formatter.hpp"
#include "output_format_type.hpp"
#include "request.hpp"

#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace transferase {

struct genomic_interval;
struct genome_index;
struct level_element_t;
struct level_element_covered_t;
enum class output_format_t : std::uint8_t;

template <typename T> struct writer_base {
  static constexpr auto max_digits = 10;
  static constexpr auto max_cols = 3;
  // ADS: below = (max_digits) x (max_methylomes) x (max_cols)
  // <= 10 x 200 x 3;
  static constexpr auto output_buffer_size{
    max_digits * request::max_methylomes_per_request * max_cols};

  const std::string &outfile;
  const genome_index &index;
  const output_format_t out_fmt;
  const std::vector<std::string> &names;
  const std::uint32_t min_reads{};
  const std::vector<std::uint32_t> &n_cpgs;

  writer_base(const std::string &outfile, const genome_index &index,
              const output_format_t &out_fmt,
              const std::vector<std::string> &names,
              const std::uint32_t min_reads,
              const std::vector<std::uint32_t> &n_cpgs) :
    outfile{outfile}, index{index}, out_fmt{out_fmt}, names{names},
    min_reads{min_reads}, n_cpgs{n_cpgs} {}

  // clang-format off
  auto self() noexcept -> T & {return static_cast<T &>(*this);}
  auto self() const noexcept -> const T & {return static_cast<const T &>(*this);}
  // clang-format on

  [[nodiscard]] auto
  write_bedlike(const auto &levels, const level_element_mode mode)
    const noexcept -> std::error_code {
    return self().write_bedlike_impl(levels, mode);
  }

  [[nodiscard]] auto
  write_dataframe(const auto &levels, const level_element_mode mode,
                  const char rowname_delim = '.',
                  const bool write_header = true) const noexcept
    -> std::error_code {
    return self().write_dataframe_impl(levels, mode, rowname_delim,
                                       write_header);
  }

  [[nodiscard]] auto
  write_dfscores(const auto &levels, const char rowname_delim = '.',
                 const bool write_header = true) const noexcept
    -> std::error_code {
    return self().write_dfscores_impl(levels, rowname_delim, write_header);
  }

  [[nodiscard]] auto
  write_output(const auto &levels) const noexcept -> std::error_code {
    switch (out_fmt) {
    case output_format_t::counts:
      return write_bedlike(levels, level_element_mode::counts);
    case output_format_t::classic:
      return write_bedlike(levels, level_element_mode::classic);
    case output_format_t::scores:
      return write_dfscores(levels, '\t', false);
    case output_format_t::dfcounts:
      return write_dataframe(levels, level_element_mode::counts);
    case output_format_t::dfclassic:
      return write_dataframe(levels, level_element_mode::classic);
    case output_format_t::dfscores:
      return write_dfscores(levels);
    }
    std::unreachable();
  }
};

}  // namespace transferase

#endif  // LIB_WRITER_BASE_HPP_
