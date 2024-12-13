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

#include <cpg_index_meta.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

TEST(cpg_index_meta_test, basic_assertions) {
  cpg_index_meta cim;
  EXPECT_EQ(cim.get_n_cpgs_chrom(), std::vector<std::uint32_t>());
  cim.chrom_offset = {0, 1000, 10000};
  cim.n_cpgs = 11000;
  EXPECT_EQ(cim.get_n_cpgs_chrom(),
            std::vector<std::uint32_t>({1000, 9000, 1000}));
  cim.chrom_offset = {0};
  cim.n_cpgs = 0;
  EXPECT_EQ(cim.get_n_cpgs_chrom(), std::vector<std::uint32_t>({0}));
}
