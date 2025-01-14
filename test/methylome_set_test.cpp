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

#include <methylome.hpp>

#include <gtest/gtest.h>

#include <cstdint>  // for std::uint32_t
#include <format>
#include <iterator>  // for std::size
#include <memory>    // for std::unique_ptr, std::shared_ptr
#include <ranges>    // for std::views
#include <string>
#include <tuple>  // for std::get
#include <unordered_map>
#include <vector>

using namespace transferase;  // NOLINT

class methylome_set_mock : public ::testing::Test {
protected:
  auto
  SetUp() -> void override {
    max_live_methylomes = 128;
    methylome_directory = "data";
    methylome_set_ptr =
      std::make_unique<methylome_set>(methylome_directory, max_live_methylomes);
  }

  auto
  TearDown() -> void override {}

  // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
  std::uint32_t max_live_methylomes;
  std::string methylome_directory;
  std::unique_ptr<methylome_set> methylome_set_ptr;
  // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)
};

TEST_F(methylome_set_mock, get_methylome_existing_accession) {
  std::error_code ec;
  const auto meth_ptr = methylome_set_ptr->get_methylome("SRX012345", ec);
  EXPECT_FALSE(ec);
  EXPECT_EQ(std::size(methylome_set_ptr->accession_to_methylome), 1);
}

TEST_F(methylome_set_mock, get_methylome_invalid_accession) {
  std::error_code ec;
  const auto meth_ptr =
    methylome_set_ptr->get_methylome("invalid.accession", ec);
  EXPECT_EQ(meth_ptr, nullptr);
  EXPECT_EQ(ec, methylome_error_code::invalid_accession);
}

TEST_F(methylome_set_mock, methylome_file_not_found) {
  std::error_code ec;
  const auto meth_ptr = methylome_set_ptr->get_methylome("DRX000000", ec);
  EXPECT_EQ(meth_ptr, nullptr);
  EXPECT_EQ(ec, methylome_set_error_code::methylome_not_found);
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
      std::make_unique<methylome_set>(methylome_directory, max_live_methylomes);
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
    std::error_code ec;
    const auto meth_ptr = methylome_set_ptr->get_methylome(accession, ec);
    EXPECT_EQ(ec, std::error_code{});
    EXPECT_NE(meth_ptr, nullptr);
  }
}

TEST_F(methylome_set_lutions, get_methylome_get_already_loaded) {
  std::error_code ec;
  const auto meth_ptr = methylome_set_ptr->get_methylome(accessions.back(), ec);
  EXPECT_EQ(ec, std::error_code{});
  EXPECT_NE(meth_ptr, nullptr);
}
