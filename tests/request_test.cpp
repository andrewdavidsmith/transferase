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

#include <request.hpp>

#include <query_interval_set.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <iostream>

// Demonstrate some basic assertions.
TEST(request_header, basic_assertions) {
  request_header hdr;
  EXPECT_EQ(hdr, request_header{});

  const auto rq_type = request_header::request_type::counts;
  hdr = request_header{"SRX012345", 12345, rq_type};
  EXPECT_TRUE(hdr.is_valid_type());

  static constexpr auto buf_size{1024};
  std::vector<char> buf(buf_size);
  const auto req = request{2, {{0, 1}, {3, 4}}};
  const auto res = to_chars(buf.data(), buf.data() + std::size(buf), req);
  EXPECT_FALSE(res.error);
  EXPECT_EQ(std::string(buf.data(), res.ptr), "2\n");
}

TEST(request, basic_assertions) {
  const auto req = request{
    3,
    {{1, 3}, {10, 20}, {100, 321}},
  };
  EXPECT_EQ(req.n_intervals, 3);
}

TEST(request, get_offsets) {
  constexpr char expected[] = {
    1, 0, 0, 0, 3, 0, 0, 0, 10, 0, 0, 0, 20, 0, 0, 0, 100, 0, 0, 0, 65, 1, 0, 0,
  };
  auto req = request{
    3,
    {{1, 3}, {10, 20}, {100, 321}},
  };
  const auto n_bytes = req.get_offsets_n_bytes();
  EXPECT_EQ(n_bytes, 24) << "failure in request::get_offsets_n_bytes()";
  const char *data = req.get_offsets_data();
  const char *data_end = data + n_bytes;
  const auto data_vec = std::vector<char>(data, data_end);
  EXPECT_STREQ(data_vec.data(), expected);
}

TEST(bins_request, basic_assertions) {
  const auto req = bins_request{100};
  EXPECT_EQ(req.bin_size, 100);
}
