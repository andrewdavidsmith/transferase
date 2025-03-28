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

#include "unit_test_utils.hpp"

#include <remote_data_resource.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>  // for std::size
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

using namespace transferase;  // NOLINT

TEST(remote_data_resource_test, form_index_target_stem_success) {
  static constexpr auto genome = "hg38";
  static constexpr auto expected = "indexes/hg38";
  remote_data_resource rdr;
  const std::string index_target_stem = rdr.form_index_target_stem(genome);
  EXPECT_EQ(index_target_stem, expected)
    << index_target_stem << "\t" << expected;
}

TEST(remote_data_resource_test, form_metadata_target_stem_success) {
  static constexpr auto expected = "metadata/latest/metadata.txt";
  remote_data_resource rdr;
  const std::string metadata_target_stem = rdr.form_metadata_target();
  EXPECT_EQ(metadata_target_stem, expected)
    << metadata_target_stem << "\t" << expected;
}

TEST(remote_data_resource_test, form_url_success) {
  static constexpr auto filename = "/filename";
  static constexpr auto path = "data";
  static constexpr auto port = "1234";
  static constexpr auto hostname = "asdf";
  const remote_data_resource rdr{hostname, port, path};
  const std::string url = rdr.form_url(filename);
  EXPECT_EQ(url, std::format("{}:{}{}", hostname, port, filename));
}
