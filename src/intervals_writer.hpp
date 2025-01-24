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

#ifndef SRC_INTERVAL_WRITER_HPP_
#define SRC_INTERVAL_WRITER_HPP_

#include "output_format_type.hpp"
#include "writer_base.hpp"

#include <string>
#include <system_error>
#include <vector>

namespace transferase {

struct genomic_interval;
struct genome_index;

struct intervals_writer : public writer_base<intervals_writer> {
  const std::vector<genomic_interval> &intervals;
  intervals_writer(const std::string &outfile, const genome_index &index,
                   const output_format_t &out_fmt,
                   const std::vector<std::string> &names,
                   const std::vector<genomic_interval> &intervals) :
    writer_base{outfile, index, out_fmt, names}, intervals{intervals} {}

  [[nodiscard]] auto
  write_impl(const auto &levels) const noexcept -> std::error_code;

  [[nodiscard]] auto
  write_bedgraph_impl(const auto &levels) const noexcept -> std::error_code;

  [[nodiscard]] auto
  write_dataframe_impl(const auto &levels) const noexcept -> std::error_code;
};

}  // namespace transferase

#endif  // SRC_INTERVAL_WRITER_HPP_
