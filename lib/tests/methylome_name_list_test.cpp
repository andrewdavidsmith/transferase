/* MIT License
 *
 * Copyright (c) 2025 Andrew D Smith
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

#include <methylome_name_list.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <iterator>  // for std::size
#include <string>
#include <system_error>
#include <vector>

using namespace transferase;  // NOLINT
namespace fs = std::filesystem;

class methylome_name_list_mock : public ::testing::Test {
protected:
  auto
  SetUp() -> void override {
    methylome_list_filename = fs::path{"data"} / fs::path{"lutions"} /
                              methylome_name_list::get_default_filename();
  }
  auto
  TearDown() -> void override {}

public:
  // cppcheck-suppress unusedStructMember
  std::string methylome_list_filename;
  std::uint32_t n_lutions_available{3};
  std::uint32_t n_lutions_tissues{3};
};

TEST_F(methylome_name_list_mock, read_failure) {
  static constexpr auto list_file_mock = ".../asdf.not_json";
  std::error_code error;
  [[maybe_unused]] const methylome_name_list names =
    methylome_name_list::read(list_file_mock, error);
  EXPECT_TRUE(error);
}

TEST_F(methylome_name_list_mock, read_success) {
  std::error_code error;
  EXPECT_TRUE(fs::exists(methylome_list_filename));

  const auto names = methylome_name_list::read(methylome_list_filename, error);
  EXPECT_FALSE(error) << error.message() << '\n';
  EXPECT_EQ(std::size(names.genome_to_methylomes), n_lutions_available);
  for (const auto &d : names.genome_to_methylomes)
    EXPECT_EQ(std::size(d.second), n_lutions_tissues);
  EXPECT_EQ(std::size(names.methylome_to_genome),
            n_lutions_available * n_lutions_tissues);
}
