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

#include "cpg_index_metadata.hpp"
#include "query_element.hpp"
#include "unit_test_utils.hpp"

#include <chrom_range.hpp>
#include <cpg_index.hpp>

#include <gtest/gtest.h>

#include <iterator>  // for std::size
#include <string>
#include <vector>

using namespace transferase;  // NOLINT

TEST(cpg_index_data_test, basic_assertions) {
  cpg_index_data data;
  EXPECT_EQ(data.positions, std::vector<cpg_index_data::vec>());
  EXPECT_EQ(data.hash(), 1);
}

TEST(cpg_index_data_test, compose_cpg_index_data_filename_test) {
  static constexpr auto index_directory = "data/lutions/methylomes";
  static constexpr auto genome_name = "eFlareon";
  static constexpr auto expected_filename =
    "data/lutions/methylomes/eFlareon.cpg_idx";
  const auto filename =
    cpg_index_data::compose_filename(index_directory, genome_name);
  EXPECT_EQ(filename, expected_filename);
}

TEST(cpg_index_data_test, valid_read) {
  static constexpr auto dirname{"data"};
  static constexpr auto genome_name{"pAntiquusx"};
  std::error_code ec;
  [[maybe_unused]] const auto meta =
    cpg_index_metadata::read(dirname, genome_name, ec);
  EXPECT_FALSE(ec);
  [[maybe_unused]] const auto data =
    cpg_index_data::read(dirname, genome_name, meta, ec);
  EXPECT_FALSE(ec);
}

TEST(cpg_index_data_test, valid_write) {
  const std::filesystem::path output_file{
    generate_temp_filename("file", cpg_index_data::filename_extension)};
  cpg_index_data data;
  data.positions.push_back({1, 2, 3, 4, 5});
  std::error_code ec = data.write(output_file);
  EXPECT_EQ(ec, std::error_code{});
  const auto outfile_exists = std::filesystem::exists(output_file, ec);
  EXPECT_EQ(ec, std::error_code{});
  EXPECT_TRUE(outfile_exists);

  std::filesystem::remove(output_file, ec);
  EXPECT_EQ(ec, std::error_code{});
}

TEST(cpg_index_data_test, valid_round_trip) {
  static constexpr auto dirname{"data"};
  static constexpr auto genome_name{"pAntiquusx"};
  std::error_code ec;
  [[maybe_unused]] const auto meta =
    cpg_index_metadata::read(dirname, genome_name, ec);
  EXPECT_FALSE(ec);
  auto data = cpg_index_data::read(dirname, genome_name, meta, ec);
  EXPECT_FALSE(ec);

  const std::filesystem::path output_file{
    generate_temp_filename("file", cpg_index_data::filename_extension)};
  ec = data.write(output_file);
  EXPECT_FALSE(ec);
  const auto outfile_exists = std::filesystem::exists(output_file, ec);
  EXPECT_FALSE(ec);
  EXPECT_TRUE(outfile_exists);

  auto data2 = cpg_index_data::read(output_file, meta, ec);
  EXPECT_FALSE(ec);

  EXPECT_EQ(data.positions, data2.positions);

  std::filesystem::remove(output_file, ec);
  EXPECT_FALSE(ec);
}

TEST(cpg_index_data_test, invalid_read) {
  static constexpr auto index_dir{"data"};
  static constexpr auto assembly{"invalid_index_file"};
  std::error_code ec;
  const auto index = cpg_index::read(index_dir, assembly, ec);
  EXPECT_TRUE(ec);
  EXPECT_EQ(std::size(index.meta.chrom_order), 0);
  EXPECT_EQ(std::size(index.meta.chrom_size), 0);
  EXPECT_EQ(std::size(index.data.positions), 0);
}

TEST(cpg_index_data_test, valid_make_query_within_chrom) {
  cpg_index_data index;
  index.positions.push_back({1, 2, 3, 4, 5});
  std::vector<chrom_range_t> queries{
    {1, 3},
    {4, 5},
  };
  const auto query = index.make_query_within_chrom(0, queries);
  const auto expected = transferase::query_container(std::vector<query_element>{
    {0, 2},
    {3, 4},
  });
  EXPECT_EQ(query, expected);
}
