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

#include "genome_index.hpp"

#include "genome_index_impl.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace transferase;  // NOLINT

TEST(genome_index_test, filename_functions) {
  static constexpr auto filename1 = "asdf";

  std::error_code ec;
  auto assembly = genome_index::parse_genome_name(filename1, ec);
  EXPECT_EQ(ec, std::errc::invalid_argument);
  EXPECT_EQ(assembly, std::string{});

  static constexpr auto valid_ref_genome = "asdf.faa.gz";
  assembly = genome_index::parse_genome_name(valid_ref_genome, ec);
  EXPECT_FALSE(ec);
  EXPECT_EQ(assembly, "asdf");
}

TEST(genome_index_test, mmap_genome_non_existent_file) {
  auto gf = mmap_genome("non_existent_file.txt");
  EXPECT_TRUE(gf.ec);
  EXPECT_EQ(gf.data, nullptr);
  EXPECT_EQ(gf.sz, 0u);
}

TEST(genome_index_test, mmap_genome_valid_file) {
  static constexpr auto filename = "data/tProrsus1.fa";
  auto gf = mmap_genome(filename);
  EXPECT_FALSE(gf.ec);
  EXPECT_NE(gf.data, nullptr);
  EXPECT_GT(gf.sz, 0u);
  [[maybe_unused]] const auto unused = cleanup_mmap_genome(gf);

  // Create a temporary file with some content
  std::ofstream outfile("test_genome.txt");
  outfile << ">chr1\nACGT\n>chr2\nGGCC\n";
  outfile.close();

  gf = mmap_genome("test_genome.txt");
  EXPECT_FALSE(gf.ec);
  EXPECT_NE(gf.data, nullptr);
  EXPECT_GT(gf.sz, 0u);

  [[maybe_unused]] const auto err = cleanup_mmap_genome(gf);
  const bool remove_ok = std::filesystem::remove("test_genome.txt");
  EXPECT_TRUE(remove_ok);
}

TEST(genome_index_test, mmap_genome_invalid_file) {
  static constexpr auto invalid_filename = "/not_a_file";
  auto gf = mmap_genome(invalid_filename);
  EXPECT_TRUE(gf.ec);
  EXPECT_EQ(gf.data, nullptr);
  EXPECT_EQ(gf.sz, 0u);
}

TEST(genome_index_test, cleanup_mmap_genome_valid_unmap) {
  static constexpr auto filename = "data/tProrsus1.fa";
  auto gf = mmap_genome(filename);
  ASSERT_FALSE(gf.ec);
  ASSERT_NE(gf.data, nullptr);
  ASSERT_GT(gf.sz, 0u);
  const auto err = cleanup_mmap_genome(gf);
  EXPECT_FALSE(err);
}

TEST(genome_index_test, cleanup_mmap_genome_valid_data) {
  // Create a temporary file with some content
  std::ofstream outfile("test_genome.txt");
  outfile << ">chr1\nACGT\n>chr2\nGGCC\n";
  outfile.close();

  auto gf = mmap_genome("test_genome.txt");
  EXPECT_FALSE(gf.ec);

  const auto err = cleanup_mmap_genome(gf);
  EXPECT_FALSE(err);
  const bool remove_ok = std::filesystem::remove("test_genome.txt");
  EXPECT_TRUE(remove_ok);
}

TEST(genome_index_test, cleanup_mmap_genome_invalid_unmap) {
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

TEST(genome_index_test, get_chrom_name_starts_valid_data) {
  {
    constexpr auto data = ">chrom1\nATCG\n>chrom2\nGCTA";
    const auto starts = get_chrom_name_starts(data, strlen(data));
    const auto expected = std::vector<std::size_t>{0, 13};
    EXPECT_EQ(starts, expected);
  }
  {
    constexpr auto data = ">chr1\nACGT\n>chr2\nGGCC\n";
    const auto starts = get_chrom_name_starts(data, strlen(data));
    EXPECT_EQ(starts.size(), 2u);
    EXPECT_EQ(starts[0], 0u);
    EXPECT_EQ(starts[1], 11u);
  }
}

TEST(genome_index_test, get_chrom_name_stops_valid_data) {
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
    EXPECT_EQ(std::size(stops), 2u);
    EXPECT_EQ(stops[0], 5u);
    EXPECT_EQ(stops[1], 16u);
  }
}

TEST(genome_index_test, get_chroms_valid_data) {
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
    EXPECT_EQ(chroms.size(), 2u);
    EXPECT_EQ(chroms[0], "ACGT\n");
    EXPECT_EQ(chroms[1], "GGCC\n");
  }
}

TEST(genome_index_test, make_genome_index_valid_genome_file) {
  std::error_code ec;
  const auto index = genome_index::make_genome_index("data/tProrsus1.fa", ec);
  EXPECT_FALSE(ec);
  EXPECT_GT(std::size(index.meta.chrom_order), 0u);
  EXPECT_GT(std::size(index.meta.chrom_size), 0u);
  EXPECT_GT(std::size(index.data.positions), 0u);
}

TEST(genome_index_test, initialize_genome_index_invalid_genome_file) {
  std::error_code ec;
  const auto index = genome_index::make_genome_index("data/intervals.bed", ec);
  EXPECT_TRUE(ec);
  EXPECT_EQ(std::size(index.meta.chrom_order), 0u);
  EXPECT_EQ(std::size(index.meta.chrom_size), 0u);
  EXPECT_EQ(std::size(index.data.positions), 0u);
}
