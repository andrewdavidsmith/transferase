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

#ifndef SRC_UTILITIES_HPP_
#define SRC_UTILITIES_HPP_

/*
  Functions declared here are used by multiple source files
 */

#include <ostream>
#include <chrono>

#include "genomic_interval.hpp"
#include "cpg_index.hpp"
#include "methylome.hpp"

auto
write_intervals(std::ostream &out, const cpg_index &index,
                const std::vector<genomic_interval> &gis,
                const std::vector<counts_res> &results) -> void;

[[nodiscard]] inline auto
duration(const auto start, const auto stop) {
  const auto d = stop - start;
  return std::chrono::duration_cast<std::chrono::duration<double>>(d).count();
}

#endif  // SRC_UTILITIES_HPP_
