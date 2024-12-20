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

#include <methylome_set.hpp>
#include <xfrase_error.hpp>

#include <gtest/gtest.h>

#include <cstdint>  // for std::uint32_t
#include <format>
#include <iterator>  // for std::size
#include <memory>    // for std::unique_ptr, std::shared_ptr
#include <string>
#include <tuple>  // for std::get
#include <unordered_map>
#include <vector>

TEST(methylome_set_test, invalid_accession) {
  auto res = is_valid_accession("invalid.accession");
  EXPECT_FALSE(res);
  res = is_valid_accession("invalid/accession");
  EXPECT_FALSE(res);
}

TEST(methylome_set_test, valid_accessions) {
  auto res = is_valid_accession("eFlareon_brain");
  EXPECT_TRUE(res);
  res = is_valid_accession("SRX012345");
  EXPECT_TRUE(res);
}

class methylome_set_mock : public ::testing::Test {
protected:
  auto
  SetUp() -> void override {
    max_live_methylomes = 128;
    methylome_directory = "data";
    methylome_set_ptr =
      std::make_unique<methylome_set>(max_live_methylomes, methylome_directory);
  }

  auto
  TearDown() -> void override {}

  std::uint32_t max_live_methylomes;
  std::string methylome_directory;
  std::unique_ptr<methylome_set> methylome_set_ptr;
};

TEST_F(methylome_set_mock, get_methylome_existing_accession) {
  const auto [meth_ptr, meta_ptr, ec] =
    methylome_set_ptr->get_methylome("SRX012345");
  EXPECT_EQ(std::size(methylome_set_ptr->accession_to_methylome), 1);
}

TEST_F(methylome_set_mock, get_methylome_invalid_accession) {
  const auto [unused_meth_ptr, unused_meta_ptr, ec] =
    methylome_set_ptr->get_methylome("invalid.accession");
  EXPECT_EQ(unused_meth_ptr, nullptr);
  EXPECT_EQ(unused_meth_ptr, nullptr);
  EXPECT_EQ(ec, methylome_set_code::invalid_accession);
}

TEST_F(methylome_set_mock, methylome_file_not_found) {
  const auto [unused_meth_ptr, unused_meta_ptr, err] =
    methylome_set_ptr->get_methylome("DRX000000");
  EXPECT_EQ(err, methylome_set_code::methylome_file_not_found);
}

class methylome_set_lutions : public ::testing::Test {
protected:
  auto
  SetUp() -> void override {
    const auto species = std::vector{
      "eFlareon",
      "eJolteon",
      "eVaporeon",
    };
    const auto tissues = std::vector{
      "brain",
      "tail",
      "ear",
    };
    accessions =
      std::views::cartesian_product(species, tissues) |
      std::views::transform([](const auto &both) {
        return std::format("{}_{}", std::get<0>(both), std::get<1>(both));
      }) |
      std::ranges::to<std::vector>();

    max_live_methylomes = 3;
    methylome_directory = "data/lutions/methylomes";
    methylome_set_ptr =
      std::make_unique<methylome_set>(max_live_methylomes, methylome_directory);
  }

  auto
  TearDown() -> void override {}

  std::uint32_t max_live_methylomes;
  std::string methylome_directory;
  std::unique_ptr<methylome_set> methylome_set_ptr;
  std::vector<std::string> accessions;
};

TEST_F(methylome_set_lutions, get_methylome_more_than_max_methylomes) {
  for (auto const &accession : accessions) {
    const auto [meth, meta, ec] = methylome_set_ptr->get_methylome(accession);
    EXPECT_EQ(ec, methylome_set_code::ok);
    EXPECT_NE(meth, nullptr);
    EXPECT_NE(meta, nullptr);
  }
}

TEST_F(methylome_set_lutions, get_methylome_get_already_loaded) {
  const auto [meth, meta, ec] =
    methylome_set_ptr->get_methylome(accessions.back());
  EXPECT_EQ(ec, methylome_set_code::ok);
  EXPECT_NE(meth, nullptr);
  EXPECT_NE(meta, nullptr);
}

TEST_F(methylome_set_lutions, get_methylome_get_inconsistent_state) {
  // Put the methylome_set in an inconsistent state
  const auto [meth, meta, ec] =
    methylome_set_ptr->get_methylome(accessions.back());
  auto to_remove =
    methylome_set_ptr->accession_to_methylome.find(accessions.back());
  EXPECT_NE(to_remove, std::cend(methylome_set_ptr->accession_to_methylome));
  if (to_remove != std::cend(methylome_set_ptr->accession_to_methylome))
    methylome_set_ptr->accession_to_methylome.erase(to_remove);

  const auto [meth_other, meta_other, ec_other] =
    methylome_set_ptr->get_methylome(accessions.back());
  EXPECT_EQ(ec_other, methylome_set_code::methylome_already_live);
  EXPECT_EQ(meth_other, nullptr);
  EXPECT_EQ(meta_other, nullptr);
}
