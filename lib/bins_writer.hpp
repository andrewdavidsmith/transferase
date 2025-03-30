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

#ifndef LIB_BINS_WRITER_HPP_
#define LIB_BINS_WRITER_HPP_

#include "writer_base.hpp"

#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

namespace transferase {

struct genome_index;
enum class output_format_t : std::uint8_t;
enum class level_element_mode : std::uint8_t;

struct bins_writer : public writer_base<bins_writer> {
  const std::uint32_t bin_size;
  bins_writer(const std::string &outfile, const genome_index &index,
              const output_format_t out_fmt,
              const std::vector<std::string> &names,
              const std::uint32_t &min_reads,
              const std::vector<std::uint32_t> &n_cpgs,
              const std::uint32_t bin_size) :
    writer_base{outfile, index, out_fmt, names, min_reads, n_cpgs},
    bin_size{bin_size} {}

  // prevent copy and move
  // clang-format off
  bins_writer(const bins_writer &) = delete;
  auto operator=(const bins_writer &) -> bins_writer & = delete;
  bins_writer(bins_writer &&) noexcept = delete;
  auto operator=(bins_writer &&) noexcept -> bins_writer & = delete;
  ~bins_writer() = default;
  // clang-format on

  [[nodiscard]] auto
  write_bedlike_impl(const auto &levels, const level_element_mode mode)
    const noexcept -> std::error_code;

  [[nodiscard]] auto
  write_dataframe_impl(
    const auto &levels, const level_element_mode mode, const char rowname_delim,
    const bool write_header) const noexcept -> std::error_code;

  [[nodiscard]] auto
  write_dfscores_impl(const auto &levels, const char rowname_delim,
                      const bool write_header) const noexcept
    -> std::error_code;
};

}  // namespace transferase

#endif  // LIB_BINS_WRITER_HPP_
