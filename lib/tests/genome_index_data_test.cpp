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

#include <genome_index_data.hpp>

#include <genome_index_data_impl.hpp>
#include <genome_index_metadata.hpp>
#include <query_element.hpp>

#include "unit_test_utils.hpp"

#include <chrom_range.hpp>
#include <genome_index.hpp>

#include <gtest/gtest.h>

#include <iterator>  // for std::size
#include <string>
#include <vector>

using namespace transferase;  // NOLINT

TEST(genome_index_data_test, empty_data) {
  genome_index_data data;
  EXPECT_EQ(data.positions, std::vector<genome_index_data::vec>());
  EXPECT_EQ(data.hash(), 1u);
}

TEST(genome_index_data_test, compose_genome_index_data_filename_test) {
  static constexpr auto index_directory = "data/lutions/methylomes";
  static constexpr auto genome_name = "eFlareon";
  static constexpr auto expected_filename =
    "data/lutions/methylomes/eFlareon.cpg_idx";
  const auto filename =
    genome_index_data::compose_filename(index_directory, genome_name);
  EXPECT_EQ(filename, expected_filename);
}

TEST(genome_index_data_test, valid_read) {
  static constexpr auto dirname{"data"};
  static constexpr auto genome_name{"pAntiquusx"};
  std::error_code ec;
  [[maybe_unused]] const auto meta =
    genome_index_metadata::read(dirname, genome_name, ec);
  EXPECT_FALSE(ec);
  [[maybe_unused]] const auto data =
    genome_index_data::read(dirname, genome_name, meta, ec);
  EXPECT_FALSE(ec);
}

TEST(genome_index_data_test, invalid_read_file_does_not_exist) {
  static constexpr auto dirname{"data"};
  static constexpr auto genome_name_ok{"pAntiquusx"};
  static constexpr auto genome_name_bad{"pAntiquusy"};
  std::error_code metadata_ec;
  [[maybe_unused]] const auto meta =
    genome_index_metadata::read(dirname, genome_name_ok, metadata_ec);
  EXPECT_FALSE(metadata_ec);
  std::error_code data_ec;
  [[maybe_unused]] const auto data =
    genome_index_data::read(dirname, genome_name_bad, meta, data_ec);
  EXPECT_EQ(data_ec, std::errc::no_such_file_or_directory);
}

TEST(genome_index_data_test, valid_write) {
  const std::filesystem::path output_file{
    generate_temp_filename("file", genome_index_data::filename_extension)};
  genome_index_data data;
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
  data.positions.push_back({1, 2, 3, 4, 5});
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
  std::error_code ec = data.write(output_file);
  EXPECT_EQ(ec, std::error_code{});
  const auto outfile_exists = std::filesystem::exists(output_file, ec);
  EXPECT_EQ(ec, std::error_code{});
  EXPECT_TRUE(outfile_exists);

  const bool remove_ok = std::filesystem::remove(output_file, ec);
  EXPECT_EQ(ec, std::error_code{});
  EXPECT_TRUE(remove_ok);
}

TEST(genome_index_data_test, write_bad_output_directory) {
  const std::filesystem::path output_file{"/etc/bad/directory/file.txt"};
  genome_index_data data;
  std::error_code ec = data.write(output_file);
  EXPECT_EQ(ec, std::errc::no_such_file_or_directory);
  const bool remove_ok = std::filesystem::remove(output_file, ec);
  EXPECT_EQ(ec, std::error_code{});
  EXPECT_FALSE(remove_ok);
}

TEST(genome_index_data_test, write_bad_output_file) {
  const std::filesystem::path output_file{"/proc/bad_file.txt"};
  genome_index_data data;
  std::error_code ec = data.write(output_file);
  EXPECT_EQ(ec, std::errc::no_such_file_or_directory);
  const bool remove_ok = std::filesystem::remove(output_file, ec);
  EXPECT_EQ(ec, std::error_code{});
  EXPECT_FALSE(remove_ok);
}

TEST(genome_index_data_test, valid_round_trip) {
  static constexpr auto dirname{"data"};
  static constexpr auto genome_name{"pAntiquusx"};
  std::error_code ec;
  [[maybe_unused]] const auto meta =
    genome_index_metadata::read(dirname, genome_name, ec);
  EXPECT_FALSE(ec);
  auto data = genome_index_data::read(dirname, genome_name, meta, ec);
  EXPECT_FALSE(ec);

  const std::filesystem::path output_file{
    generate_temp_filename("file", genome_index_data::filename_extension)};
  ec = data.write(output_file);
  EXPECT_FALSE(ec);
  const auto outfile_exists = std::filesystem::exists(output_file, ec);
  EXPECT_FALSE(ec);
  EXPECT_TRUE(outfile_exists);

  auto data2 = genome_index_data::read(output_file, meta, ec);
  EXPECT_FALSE(ec);

  EXPECT_EQ(data.positions, data2.positions);

  const bool remove_ok = std::filesystem::remove(output_file, ec);
  EXPECT_FALSE(ec);
  EXPECT_TRUE(remove_ok);
}

TEST(genome_index_data_test, invalid_read) {
  static constexpr auto index_dir{"data"};
  static constexpr auto assembly{"invalid_index_file"};
  std::error_code ec;
  const auto index = genome_index::read(index_dir, assembly, ec);
  EXPECT_TRUE(ec);
  EXPECT_EQ(std::size(index.meta.chrom_order), 0u);
  EXPECT_EQ(std::size(index.meta.chrom_size), 0u);
  EXPECT_EQ(std::size(index.data.positions), 0u);
}

class genome_index_data_make_query_success : public ::testing::Test {
protected:
  auto
  SetUp() -> void override {
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
    some_wide_range = chrom_range_t{0, 100'000};
    data = genome_index_data({{1, 2, 3, 4, 5}});
    queries = std::vector{
      chrom_range_t{1, 3},
      chrom_range_t{4, 5},
    };
    expected = transferase::query_container(std::vector{
      query_element{0, 2},
      query_element{3, 4},
    });
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
  }
  auto
  TearDown() -> void override {}

public:
  chrom_range_t some_wide_range;
  genome_index_data data;
  std::vector<chrom_range_t> queries;
  query_container expected;
};

TEST_F(genome_index_data_make_query_success, valid_make_query_within_chrom) {
  const auto query =
    transferase::make_query_within_chrom(data.positions[0], queries);
  EXPECT_EQ(query, expected);
  EXPECT_LE(some_wide_range, queries[0]);
}

class genome_index_read_non_existent : public ::testing::Test {
protected:
  auto
  SetUp() -> void override {
    valid_index_dir = "data";
    non_existent_genome_name = "asdfasdfasdf";
    possibly_valid_index_dir = "/etc/";
    invalid_genome_index_data_file = "passwd";
  }
  auto
  TearDown() -> void override {}

public:
  std::string valid_index_dir;
  std::string non_existent_genome_name;
  std::string possibly_valid_index_dir;
  std::string invalid_genome_index_data_file;
};

TEST_F(genome_index_read_non_existent, read_non_existent) {
  std::error_code ec;
  [[maybe_unused]] const auto index =
    genome_index::read(valid_index_dir, non_existent_genome_name, ec);
  EXPECT_TRUE(ec);
}

TEST_F(genome_index_read_non_existent, read_invalid) {
  std::error_code ec;
  [[maybe_unused]] const auto index = genome_index::read(
    possibly_valid_index_dir, invalid_genome_index_data_file, ec);
  EXPECT_TRUE(ec);
}
