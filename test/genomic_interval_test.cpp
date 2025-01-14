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

#include <genome_index.hpp>
#include <genome_index_metadata.hpp>
#include <genomic_interval_impl.hpp>

#include <gtest/gtest.h>

#include <iterator>
#include <string>
#include <unordered_map>

using namespace transferase;  // NOLINT

TEST(genomic_interval_test, basic_assertions) {
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
  static constexpr auto index_dir{"data"};
  static constexpr auto assembly{"tProrsus1"};
  static constexpr auto intervals_file{"data/tProrsus1_intervals.bed"};
  std::error_code ec;
  const auto index = genome_index::read(index_dir, assembly, ec);
  EXPECT_FALSE(ec);
  const auto intervals = genomic_interval::read(index, intervals_file, ec);
  EXPECT_FALSE(ec);
  EXPECT_EQ(std::size(intervals), 20);
  EXPECT_EQ(intervals[0].start, 6595);
  EXPECT_EQ(intervals[0].stop, 6890);
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}

TEST(genomic_interval_test, read_non_existent_file) {
  static constexpr auto index_dir{"data"};
  static constexpr auto assembly{"asdfasdfasdf"};
  std::error_code ec;
  [[maybe_unused]] const auto index =
    genome_index::read(index_dir, assembly, ec);
  EXPECT_TRUE(ec);
}

TEST(genomic_interval_test, read_invalid_file) {
  static constexpr auto index_dir{"/etc/"};
  static constexpr auto assembly{"passwd"};
  std::error_code ec;
  [[maybe_unused]] const auto index =
    genome_index::read(index_dir, assembly, ec);
  EXPECT_TRUE(ec);
}

// Test cases
TEST(genomic_interval_test, valid_input) {
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
  genome_index_metadata meta;
  meta.chrom_index["chr1"] = 0;
  meta.chrom_size.push_back(100000);
  std::error_code ec;
  const auto result = parse(meta, "chr1 100 200", ec);
  EXPECT_FALSE(ec);
  EXPECT_EQ(result.ch_id, 0);
  EXPECT_EQ(result.start, 100);
  EXPECT_EQ(result.stop, 200);
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}

TEST(genomic_interval_test, valid_input_with_tabs) {
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
  genome_index_metadata meta;
  meta.chrom_index["chr1"] = 0;
  meta.chrom_size.push_back(100000);
  std::error_code ec;
  const auto result = parse(meta, "chr1\t100\t200", ec);
  EXPECT_FALSE(ec);
  EXPECT_EQ(result.ch_id, 0);
  EXPECT_EQ(result.start, 100);
  EXPECT_EQ(result.stop, 200);
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}

TEST(genomic_interval_test, missing_chromosome_name) {
  genome_index_metadata meta;
  std::error_code ec;
  [[maybe_unused]] const auto result = parse(meta, "100 200", ec);
  EXPECT_TRUE(ec);
  EXPECT_EQ(ec, genomic_interval_error_code::error_parsing_bed_line);
}

TEST(genomic_interval_test, invalid_start_position) {
  genome_index_metadata meta;
  std::error_code ec;
  [[maybe_unused]] const auto result = parse(meta, "chr1 abc 200", ec);
  EXPECT_TRUE(ec);
  EXPECT_EQ(ec, genomic_interval_error_code::error_parsing_bed_line);
}

TEST(genomic_interval_test, non_existent_chromosome_name) {
  genome_index_metadata meta;
  std::error_code ec;
  [[maybe_unused]] const auto result = parse(meta, "chr2 100 200", ec);
  EXPECT_TRUE(ec);
  EXPECT_EQ(ec, genomic_interval_error_code::chrom_name_not_found_in_index);
}

TEST(genomic_interval_test, stop_position_exceeds_chromosome_size) {
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
  genome_index_metadata meta;
  meta.chrom_index["chr1"] = 0;
  meta.chrom_size.push_back(100000);
  std::error_code ec;
  auto result = parse(meta, "chr1 100 200000", ec);
  EXPECT_TRUE(ec);
  EXPECT_EQ(ec, genomic_interval_error_code::interval_past_chrom_end_in_index);
  EXPECT_EQ(result.ch_id, genomic_interval::not_a_chrom);
  EXPECT_EQ(result.start, 0);
  EXPECT_EQ(result.stop, 0);
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}
