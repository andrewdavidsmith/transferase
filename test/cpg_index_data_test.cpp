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

#include <cpg_index_data.hpp>

#include <cpg_index.hpp>
#include <cpg_index_metadata.hpp>  // IWYU pragma: keep

#include <gtest/gtest.h>

#include <cstdint>
#include <iterator>  // for std::size
#include <string>
#include <vector>

TEST(cpg_index_data_test, basic_assertions) {
  cpg_index_data ci;
  EXPECT_EQ(ci.positions, std::vector<cpg_index_data::vec>());
  EXPECT_EQ(ci.hash(), 1);
}

TEST(read_cpg_index_data_test, valid_index_file) {
  static constexpr auto index_dir{"data"};
  static constexpr auto assembly{"tProrsus1"};
  std::error_code ec;
  const auto index = cpg_index::read(index_dir, assembly, ec);
  EXPECT_FALSE(ec);
  EXPECT_GT(std::size(index.meta.chrom_order), 0);
  EXPECT_GT(std::size(index.meta.chrom_size), 0);
  EXPECT_GT(std::size(index.data.positions), 0);
}

TEST(read_cpg_index_data_test, invalid_index_file) {
  static constexpr auto index_dir{"data"};
  static constexpr auto assembly{"invalid_index_file"};
  std::error_code ec;
  const auto index = cpg_index::read(index_dir, assembly, ec);
  EXPECT_TRUE(ec);
  EXPECT_EQ(std::size(index.meta.chrom_order), 0);
  EXPECT_EQ(std::size(index.meta.chrom_size), 0);
  EXPECT_EQ(std::size(index.data.positions), 0);
}

TEST(cpg_index_data_write_test, valid_write) {
  cpg_index_data index;
  // Fill index with test data
  index.positions.push_back({1, 2, 3});
  const auto ec = index.write("test_index_file.cpg_idx");
  EXPECT_FALSE(ec);
}

TEST(cpg_index_data_get_offsets_with_chrom_test, valid_offsets) {
  cpg_index_data index;
  index.positions.push_back({1, 2, 3, 4, 5});
  std::vector<std::pair<std::uint32_t, std::uint32_t>> queries = {
    {1, 3},
    {4, 5},
  };
  const auto offsets = index.get_offsets_within_chrom(0, queries);
  std::vector<std::pair<std::uint32_t, std::uint32_t>> expected = {
    {0, 2},
    {3, 4},
  };
  EXPECT_EQ(offsets, expected);
}
