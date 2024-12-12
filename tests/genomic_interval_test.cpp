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

#include <genomic_interval.hpp>

#include <cpg_index.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <system_error>

TEST(genomic_interval_test, basic_assertions) {
  static constexpr auto index_file{"data/tProrsus1.cpg_idx"};
  static constexpr auto intervals_file{"data/tProrsus1_intervals.bed"};
  const auto [index, cim, ec] = read_cpg_index(index_file);
  const auto ret = genomic_interval::load(cim, intervals_file);

  EXPECT_FALSE(ret.ec);
  EXPECT_EQ(std::size(ret.gis), 20);
  EXPECT_EQ(ret.gis[0].start, 6595);
  EXPECT_EQ(ret.gis[0].stop, 6890);
}
