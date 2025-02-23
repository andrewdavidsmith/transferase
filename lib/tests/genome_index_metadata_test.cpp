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

#include <genome_index_metadata.hpp>

#include "unit_test_utils.hpp"

#include <config.h>  // for VERSION
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <iterator>  // std::size
#include <numeric>
#include <string>
#include <vector>

using namespace transferase;  // NOLINT

TEST(genome_index_metadata_test, basic_assertions) {
  genome_index_metadata meta;
  EXPECT_EQ(meta.get_n_cpgs_chrom(), std::vector<std::uint32_t>());
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
  meta.chrom_offset = {0, 1000, 10000};
  meta.n_cpgs = 11000;
  EXPECT_EQ(meta.get_n_cpgs_chrom(),
            std::vector<std::uint32_t>({1000, 9000, 1000}));
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
  meta.chrom_offset = {0};
  meta.n_cpgs = 0;
  EXPECT_EQ(meta.get_n_cpgs_chrom(), std::vector<std::uint32_t>({0}));
}

class genome_index_metadata_mock : public ::testing::Test {
protected:
  auto
  SetUp() -> void override {
    genome_index_dir = "data/lutions/indexes";
    species_name = "eFlareon";
  }

  auto
  TearDown() -> void override {}

public:
  std::string genome_index_dir;
  std::string species_name;
};

TEST_F(genome_index_metadata_mock, read_existing_genome_index_metadata) {
  std::error_code ec;
  const auto meta =
    genome_index_metadata::read(genome_index_dir, species_name, ec);
  EXPECT_FALSE(ec);
  EXPECT_EQ(std::size(meta.chrom_index), std::size(meta.chrom_order));
  EXPECT_EQ(std::size(meta.chrom_index), std::size(meta.chrom_size));
  EXPECT_EQ(std::size(meta.chrom_index), std::size(meta.chrom_offset));
  EXPECT_GT(meta.n_cpgs, 0);

  const auto n_cpgs_chrom = meta.get_n_cpgs_chrom();
  EXPECT_EQ(std::size(meta.chrom_index), std::size(n_cpgs_chrom));
  const auto total =
    std::reduce(std::cbegin(n_cpgs_chrom), std::cend(n_cpgs_chrom));
  EXPECT_EQ(meta.n_cpgs, total);
}

TEST_F(genome_index_metadata_mock, genome_index_metadata_read_write_read) {
  std::error_code ec;
  const auto meta =
    genome_index_metadata::read(genome_index_dir, species_name, ec);
  EXPECT_EQ(ec, std::error_code{});

  const auto tmpfile =
    generate_temp_filename("temp", genome_index_metadata::filename_extension);
  ec = meta.write(tmpfile);
  EXPECT_EQ(ec, std::error_code{});

  const auto meta_written = genome_index_metadata::read(tmpfile, ec);
  EXPECT_EQ(ec, std::error_code{});

  // ADS: gtest doesn't want to acknowledge my spaceships
  EXPECT_EQ(meta.chrom_order, meta_written.chrom_order);
  EXPECT_EQ(meta.chrom_offset, meta_written.chrom_offset);
  EXPECT_EQ(meta.chrom_size, meta_written.chrom_size);
  EXPECT_EQ(meta.index_hash, meta_written.index_hash);
  EXPECT_EQ(meta.creation_time, meta_written.creation_time);

  const auto removed = std::filesystem::remove(tmpfile, ec);
  EXPECT_FALSE(ec);
  EXPECT_TRUE(removed);
}

TEST_F(genome_index_metadata_mock,
       genome_index_metadata_read_non_existing_file) {
  static constexpr auto bad_species_name = "namekian";
  std::error_code ec;
  [[maybe_unused]] const auto meta =
    genome_index_metadata::read(genome_index_dir, bad_species_name, ec);
  EXPECT_EQ(ec, std::errc::no_such_file_or_directory);
}

TEST_F(genome_index_metadata_mock, genome_index_metadata_get_n_bins) {
  std::error_code ec;
  const auto meta =
    genome_index_metadata::read(genome_index_dir, species_name, ec);
  EXPECT_EQ(ec, std::error_code{});
  const auto n_bins = meta.get_n_bins(1);
  EXPECT_GE(n_bins, meta.n_cpgs);
}

TEST_F(genome_index_metadata_mock, genome_index_metadata_init_env) {
  genome_index_metadata meta{};
  const auto init_env_err = meta.init_env();
  EXPECT_FALSE(init_env_err);
  EXPECT_EQ(meta.version, VERSION);
}

TEST_F(genome_index_metadata_mock, genome_index_metadata_tostring) {
  genome_index_metadata meta{};
  const auto meta_str = meta.tostring();
  EXPECT_GT(std::size(meta_str), 0);
}
