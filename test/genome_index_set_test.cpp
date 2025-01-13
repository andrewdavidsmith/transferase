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
#include "genome_index_set.hpp"

#include "genome_index_metadata.hpp"

#include <gtest/gtest.h>

#include <iterator>  // for std::size
#include <memory>    // for std::unique_ptr, std::shared_ptr
#include <string>
#include <system_error>
#include <tuple>  // for std::get

using namespace transferase;  // NOLINT

TEST(genome_index_set_test, valid_genome_index_set) {
  static constexpr auto genome_index_directory = "data";
  const auto index = genome_index_set(genome_index_directory);
  EXPECT_EQ(std::size(index.name_to_index), 0);
}

class genome_index_set_mock : public ::testing::Test {
protected:
  auto
  SetUp() -> void override {
    genome_index_directory = "data";
    genome_index_set_ptr =
      std::make_unique<genome_index_set>(genome_index_directory);
  }

  auto
  TearDown() -> void override {}

  std::string genome_index_directory;
  std::unique_ptr<genome_index_set> genome_index_set_ptr;
};

TEST_F(genome_index_set_mock, get_genome_index_metadata_genome_name) {
  static constexpr auto species = "tProrsus1";
  std::error_code ec{};
  const auto index_ptr = genome_index_set_ptr->get_genome_index(species, ec);
  EXPECT_FALSE(ec);
  EXPECT_EQ(index_ptr->meta.genome_name, species);
}

TEST_F(genome_index_set_mock, get_genome_index_set_genome_not_found) {
  std::error_code ec;
  const auto index_ptr =
    genome_index_set_ptr->get_genome_index("invalid.genome_name", ec);
  std::ignore = index_ptr;
  EXPECT_EQ(ec, genome_index_error_code::invalid_genome_name);
}
