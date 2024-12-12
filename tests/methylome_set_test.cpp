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

#include <methylome.hpp>

#include <methylome_metadata.hpp>
#include <methylome_set.hpp>
#include <mxe_error.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

class methylome_set_test : public ::testing::Test {
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

TEST_F(methylome_set_test, valid_get_methylome) {
  const auto [meth_ptr, meta_ptr, ec] =
    methylome_set_ptr->get_methylome("SRX012345");
  EXPECT_EQ(std::size(methylome_set_ptr->accession_to_methylome), 1);
}

TEST_F(methylome_set_test, invalid_accession) {
  const auto result = methylome_set_ptr->get_methylome("invalid_accession");
  EXPECT_EQ(std::get<2>(result), methylome_set_code::invalid_accession);
}

TEST_F(methylome_set_test, methylome_file_not_found) {
  const auto result = methylome_set_ptr->get_methylome("DRX000000");
  EXPECT_EQ(std::get<2>(result), methylome_set_code::methylome_file_not_found);
}
