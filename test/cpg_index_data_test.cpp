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

#include <cpg_index.hpp>
#include <cpg_index_impl.hpp>
#include <cpg_index_metadata.hpp>  // IWYU pragma: keep

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>  // for std::size
#include <string>
#include <string_view>
#include <vector>

TEST(cpg_index_test, basic_assertions) {
  cpg_index ci;
  EXPECT_EQ(ci.positions, std::vector<cpg_index::vec>());
  EXPECT_EQ(ci.hash(), 1);
}

TEST(mmap_genome_test, non_existent_file) {
  auto gf = mmap_genome("non_existent_file.txt");
  EXPECT_TRUE(gf.ec);
  EXPECT_EQ(gf.data, nullptr);
  EXPECT_EQ(gf.sz, 0);
}

TEST(mmap_genome_test, valid_file) {
  static constexpr auto filename = "data/tProrsus1.fa";
  auto gf = mmap_genome(filename);
  EXPECT_FALSE(gf.ec);
  EXPECT_NE(gf.data, nullptr);
  EXPECT_GT(gf.sz, 0);
  [[maybe_unused]] const auto unused = cleanup_mmap_genome(gf);

  // Create a temporary file with some content
  std::ofstream outfile("test_genome.txt");
  outfile << ">chr1\nACGT\n>chr2\nGGCC\n";
  outfile.close();

  gf = mmap_genome("test_genome.txt");
  EXPECT_FALSE(gf.ec);
  EXPECT_NE(gf.data, nullptr);
  EXPECT_GT(gf.sz, 0);

  [[maybe_unused]] const auto err = cleanup_mmap_genome(gf);
  std::filesystem::remove("test_genome.txt");
}

TEST(mmap_genome_test, invalid_file) {
  static constexpr auto invalid_filename = "/not_a_file";
  auto gf = mmap_genome(invalid_filename);
  EXPECT_TRUE(gf.ec);
  EXPECT_EQ(gf.data, nullptr);
  EXPECT_EQ(gf.sz, 0);
}

TEST(cleanup_mmap_genome_test, valid_unmap) {
  static constexpr auto filename = "data/tProrsus1.fa";
  auto gf = mmap_genome(filename);
  ASSERT_FALSE(gf.ec);
  ASSERT_NE(gf.data, nullptr);
  ASSERT_GT(gf.sz, 0);
  const auto err = cleanup_mmap_genome(gf);
  EXPECT_FALSE(err);
}

TEST(cleanup_mmap_genome_test, valid_data) {
  // Create a temporary file with some content
  std::ofstream outfile("test_genome.txt");
  outfile << ">chr1\nACGT\n>chr2\nGGCC\n";
  outfile.close();

  auto gf = mmap_genome("test_genome.txt");
  EXPECT_FALSE(gf.ec);

  const auto err = cleanup_mmap_genome(gf);
  EXPECT_FALSE(err);
  std::filesystem::remove("test_genome.txt");
}

TEST(cleanup_mmap_genome_test, invalid_unmap) {
  genome_file gf = {std::error_code{}, nullptr, 0};
  auto err = cleanup_mmap_genome(gf);
  EXPECT_TRUE(err);
}

TEST(get_cpgs_test, valid_chromosome) {
  const std::string_view chrom = "ACGTGCGTGCGT";
  const auto cpgs = get_cpgs(chrom);
  using T = decltype(cpgs);
  const T expected = {1, 5, 9};
  EXPECT_EQ(cpgs, expected);
}

TEST(get_cpgs_test, no_cpgs) {
  std::string_view chrom = "AACCTTGG";
  auto cpgs = get_cpgs(chrom);
  EXPECT_TRUE(cpgs.empty());
}

TEST(get_chrom_name_starts_test, valid_data) {
  {
    constexpr auto data = ">chrom1\nATCG\n>chrom2\nGCTA";
    const auto starts = get_chrom_name_starts(data, strlen(data));
    const auto expected = std::vector<std::size_t>{0, 13};
    EXPECT_EQ(starts, expected);
  }
  {
    constexpr auto data = ">chr1\nACGT\n>chr2\nGGCC\n";
    const auto starts = get_chrom_name_starts(data, strlen(data));
    EXPECT_EQ(starts.size(), 2);
    EXPECT_EQ(starts[0], 0);
    EXPECT_EQ(starts[1], 11);
  }
}

TEST(get_chrom_name_stops_test, valid_data) {
  {
    constexpr auto data = ">chrom1\nATCG\n>chrom2\nGCTA";
    const auto starts = get_chrom_name_starts(data, strlen(data));
    const auto stops = get_chrom_name_stops(starts, data, strlen(data));
    const auto expected = std::vector<std::size_t>{7, 20};
    EXPECT_EQ(stops, expected);
  }
  {
    constexpr auto data = ">chr1\nACGT\n>chr2\nGGCC\n";
    const auto starts = get_chrom_name_starts(data, strlen(data));
    const auto stops = get_chrom_name_stops(starts, data, strlen(data));
    EXPECT_EQ(std::size(stops), 2);
    EXPECT_EQ(stops[0], 5);
    EXPECT_EQ(stops[1], 16);
  }
}

// get_chroms tests
TEST(get_chroms_test, valid_data) {
  {
    const auto data = ">chrom1\nATCG\n>chrom2\nGCTA";
    const auto starts = get_chrom_name_starts(data, strlen(data));
    const auto stops = get_chrom_name_stops(starts, data, strlen(data));
    const auto chroms = get_chroms(data, strlen(data), starts, stops);
    // ADS: notice the inclusion of the newline for one chrom
    const auto expected = std::vector<std::string_view>{"ATCG\n", "GCTA"};
    EXPECT_EQ(chroms, expected);
  }
  {
    const auto data = ">chr1\nACGT\n>chr2\nGGCC\n";
    const auto starts = get_chrom_name_starts(data, strlen(data));
    const auto stops = get_chrom_name_stops(starts, data, strlen(data));
    const auto chroms = get_chroms(data, strlen(data), starts, stops);
    EXPECT_EQ(chroms.size(), 2);
    EXPECT_EQ(chroms[0], "ACGT\n");
    EXPECT_EQ(chroms[1], "GGCC\n");
  }
}

TEST(initialize_cpg_index_test, valid_genome_file) {
  const auto [index, meta, ec] = initialize_cpg_index("data/tProrsus1.fa");
  EXPECT_FALSE(ec);
  EXPECT_GT(meta.chrom_order.size(), 0);
  EXPECT_GT(meta.chrom_size.size(), 0);
  EXPECT_GT(index.positions.size(), 0);
}

TEST(initialize_cpg_index_test, invalid_genome_file) {
  const auto [index, meta, ec] = initialize_cpg_index("data/intervals.bed");
  EXPECT_TRUE(ec);
  EXPECT_EQ(meta.chrom_order.size(), 0);
  EXPECT_EQ(meta.chrom_size.size(), 0);
  EXPECT_EQ(index.positions.size(), 0);
}

TEST(read_cpg_index_test, valid_index_file) {
  const auto [index, meta, ec] = read_cpg_index("data/tProrsus1.cpg_idx");
  EXPECT_FALSE(ec);
  EXPECT_GT(meta.chrom_order.size(), 0);
  EXPECT_GT(meta.chrom_size.size(), 0);
  EXPECT_GT(index.positions.size(), 0);
}

TEST(read_cpg_index_test, invalid_index_file) {
  const auto [index, meta, ec] = read_cpg_index("invalid_index_file.cpg_idx");
  EXPECT_TRUE(ec);
  EXPECT_EQ(meta.chrom_order.size(), 0);
  EXPECT_EQ(meta.chrom_size.size(), 0);
  EXPECT_EQ(index.positions.size(), 0);
}

TEST(cpg_index_write_test, valid_write) {
  cpg_index index;
  // Fill index with test data
  index.positions.push_back({1, 2, 3});
  const auto ec = index.write("test_index_file.cpg_idx");
  EXPECT_FALSE(ec);
}

TEST(cpg_index_get_offsets_with_chrom_test, valid_offsets) {
  cpg_index index;
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
