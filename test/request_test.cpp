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
#include <request_type_code.hpp>

#include <gtest/gtest.h>

#include <cstring>
#include <string>

using namespace xfrase;  // NOLINT

TEST(request_test, basic_assertions) {
  request req;
  EXPECT_EQ(req, request{});

  const auto rq_type = request_type_code::counts;
  req = request{"SRX012345", rq_type, 0, 0};
  EXPECT_TRUE(req.is_valid_type());
}

TEST(request_test, valid_compose) {
  static constexpr auto rq_type = request_type_code::counts;
  static constexpr auto accession = "SRX012345";
  request_buffer buf;
  const request req{accession, rq_type, 0, 0};
  const auto res = compose(buf, req);
  EXPECT_FALSE(res);
  EXPECT_EQ(std::string(buf.data(), buf.data() + strlen(accession)), accession);
}

TEST(request_test, basic_assertions_bins) {
  const request req{"SRX12345", request_type_code::bin_counts, 0, 100};
  EXPECT_EQ(req.bin_size(), 100);
}
