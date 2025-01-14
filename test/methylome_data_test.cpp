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

#include <methylome_data.hpp>

#include <methylome_metadata.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <system_error>
#include <utility>

using namespace transferase;  // NOLINT

TEST(methylome_data_test, basic_assertions) {
  const std::uint32_t n_meth{65536};
  const std::uint32_t n_unmeth{65536};
  const std::uint32_t rounded_n_meth{65535};
  const std::uint32_t rounded_n_unmeth{65535};
  conditional_round_to_fit<m_count_t>(n_meth, n_unmeth);
  EXPECT_EQ(std::make_pair(n_meth, n_unmeth),
            std::make_pair(rounded_n_meth, rounded_n_unmeth));
}

TEST(methylome_data_test, valid_read) {
  static constexpr auto dirname{"data"};
  static constexpr auto methylome_name{"SRX012345"};
  static constexpr auto expected_data_size{6053};

  std::error_code ec;
  const auto meta = methylome_metadata::read(dirname, methylome_name, ec);
  EXPECT_FALSE(ec);

  const auto data = methylome_data::read(dirname, methylome_name, meta, ec);
  EXPECT_FALSE(ec);

  EXPECT_EQ(size(data), expected_data_size);
}
