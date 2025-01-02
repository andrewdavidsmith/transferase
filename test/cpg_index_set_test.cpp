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

#include "cpg_index.hpp"
#include "cpg_index_set.hpp"

#include "cpg_index_metadata.hpp"

#include <gtest/gtest.h>

#include <iterator>  // for std::size
#include <memory>    // for std::unique_ptr, std::shared_ptr
#include <string>
#include <system_error>
#include <tuple>  // for std::get

namespace xfrase {

TEST(cpg_index_set_test, valid_cpg_index_set) {
  static constexpr auto cpg_index_directory = "data";
  std::error_code ec{};
  const auto index = cpg_index_set(cpg_index_directory, ec);
  EXPECT_FALSE(ec);
  EXPECT_GT(std::size(index.assembly_to_cpg_index), 0);
}

class cpg_index_set_mock : public ::testing::Test {
protected:
  auto
  SetUp() -> void override {
    cpg_index_directory = "data";
    std::error_code unused_ec{};
    cpg_index_set_ptr =
      std::make_unique<cpg_index_set>(cpg_index_directory, unused_ec);
  }

  auto
  TearDown() -> void override {}

  std::string cpg_index_directory;
  std::unique_ptr<cpg_index_set> cpg_index_set_ptr;
};

TEST_F(cpg_index_set_mock, get_cpg_index_metadata_assembly_name) {
  static constexpr auto species = "tProrsus1";
  std::error_code ec{};
  const auto index_ptr = cpg_index_set_ptr->get_cpg_index(species, ec);
  EXPECT_FALSE(ec);
  EXPECT_EQ(index_ptr->meta.assembly, species);
}

TEST_F(cpg_index_set_mock, get_cpg_index_set_assembly_not_found) {
  std::error_code ec;
  const auto index_ptr =
    cpg_index_set_ptr->get_cpg_index("invalid.assembly", ec);
  std::ignore = index_ptr;
  EXPECT_EQ(ec, cpg_index_set_error::cpg_index_not_found);
}

}  // namespace xfrase
