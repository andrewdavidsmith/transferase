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

#include <config.h>  // for VERSION
#include <gtest/gtest.h>

#include <algorithm>  // std::equal
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>  // std::size
#include <numeric>
#include <ranges>
#include <string>
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

TEST(cpg_index_meta_test, filename_functions) {
  static constexpr auto filename1 = "asdf";
  const auto meta_filename1 = get_default_cpg_index_meta_filename(filename1);
  EXPECT_EQ(meta_filename1, "asdf.json");

  std::error_code ec;
  auto assembly = get_assembly_from_filename(filename1, ec);
  EXPECT_EQ(ec, std::errc::invalid_argument);
  EXPECT_EQ(assembly, std::string{});

  static constexpr auto valid_ref_genome = "asdf.faa.gz";
  assembly = get_assembly_from_filename(valid_ref_genome, ec);
  EXPECT_FALSE(ec);
  EXPECT_EQ(assembly, "asdf");
}

class cpg_index_meta_mock : public ::testing::Test {
protected:
  auto
  SetUp() -> void override {
    cpg_index_dir = "data/lutions/indexes";
    species_name = "eFlareon";
  }

  auto
  TearDown() -> void override {}

  std::string cpg_index_dir;
  std::string species_name;
};

TEST_F(cpg_index_meta_mock, read_existing_cpg_index_meta) {
  const auto cpg_index_meta_file = std::format(
    "{}/{}{}", cpg_index_dir, species_name, cpg_index_meta::filename_extension);
  const auto [cim, ec] = cpg_index_meta::read(cpg_index_meta_file);
  EXPECT_EQ(ec, std::error_code{});
  EXPECT_EQ(std::size(cim.chrom_index), std::size(cim.chrom_order));
  EXPECT_EQ(std::size(cim.chrom_index), std::size(cim.chrom_size));
  EXPECT_EQ(std::size(cim.chrom_index), std::size(cim.chrom_offset));
  EXPECT_GT(cim.n_cpgs, 0);

  const auto n_cpgs_chrom = cim.get_n_cpgs_chrom();
  EXPECT_EQ(std::size(cim.chrom_index), std::size(n_cpgs_chrom));
  const auto total =
    std::reduce(std::cbegin(n_cpgs_chrom), std::cend(n_cpgs_chrom));
  EXPECT_EQ(cim.n_cpgs, total);
}

TEST_F(cpg_index_meta_mock, cpg_index_meta_read_write_read) {
  const auto cpg_index_meta_file = std::format(
    "{}/{}{}", cpg_index_dir, species_name, cpg_index_meta::filename_extension);
  const auto [cim, ec] = cpg_index_meta::read(cpg_index_meta_file);
  const auto cpg_index_meta_file_tmp =
    std::format("{}.tmp", cpg_index_meta_file);
  const auto write_ec = cim.write(cpg_index_meta_file_tmp);
  EXPECT_FALSE(write_ec);

  const auto [cim_written, ec_written] =
    cpg_index_meta::read(cpg_index_meta_file_tmp);

  // ADS: gtest doesn't want to acknowledge my spaceships
  EXPECT_EQ(cim.chrom_order, cim_written.chrom_order);
  EXPECT_EQ(cim.chrom_offset, cim_written.chrom_offset);
  EXPECT_EQ(cim.chrom_size, cim_written.chrom_size);
  EXPECT_EQ(cim.index_hash, cim_written.index_hash);
  EXPECT_EQ(cim.creation_time, cim_written.creation_time);

  std::error_code remove_ec{};
  [[maybe_unused]] const bool removed =
    std::filesystem::remove(cpg_index_meta_file_tmp, remove_ec);
  EXPECT_FALSE(remove_ec);
}

TEST_F(cpg_index_meta_mock, cpg_index_meta_get_n_bins) {
  const auto cpg_index_meta_file = std::format(
    "{}/{}{}", cpg_index_dir, species_name, cpg_index_meta::filename_extension);
  const auto [cim, ec] = cpg_index_meta::read(cpg_index_meta_file);
  const auto n_bins = cim.get_n_bins(1);
  EXPECT_GE(n_bins, cim.n_cpgs);
}

TEST_F(cpg_index_meta_mock, cpg_index_meta_init_env) {
  cpg_index_meta cim{};
  const auto init_env_err = cim.init_env();
  EXPECT_FALSE(init_env_err);
  EXPECT_EQ(cim.version, VERSION);
}

TEST_F(cpg_index_meta_mock, cpg_index_meta_tostring) {
  cpg_index_meta cim{};
  const auto cim_str = cim.tostring();
  EXPECT_GT(std::size(cim_str), 0);
}

TEST(cpg_index_meta_error, cpg_index_meta_error_all_values) {
  static const auto category = cpg_index_meta_error_category{};
  constexpr int lim = std::to_underlying(cpg_index_meta_error::n_values);
  for (const auto i : std::views::iota(0, lim)) {
    const std::error_code ec =
      make_error_code(static_cast<cpg_index_meta_error>(i));
    EXPECT_EQ(ec.message(), category.message(i));
  }
}
