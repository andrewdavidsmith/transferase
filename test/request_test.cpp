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

#include <string>
#include <vector>

using namespace transferase;  // NOLINT

TEST(request_test, basic_assertions) {
  request req;
  EXPECT_EQ(req, request{});

  const auto rq_type = request_type_code::intervals;
  req = request{rq_type, 0, 0, {"SRX012345"}};
  EXPECT_TRUE(req.is_valid_type());
}

TEST(request_test, valid_compose) {
  using namespace std::string_literals;  // NOLINT
  static constexpr auto rq_type = request_type_code::intervals;
  static constexpr auto mock_aux_value = 1234;
  static constexpr auto mock_index_hash = 5678;
  static constexpr auto accession = "SRX012345"s;
  request_buffer buf;
  const request req{rq_type, mock_index_hash, mock_aux_value, {accession}};
  const std::error_code compose_ec = compose(buf, req);
  EXPECT_FALSE(compose_ec);
  request req_parsed;
  const std::error_code parse_ec = parse(buf, req_parsed);
  EXPECT_FALSE(parse_ec);
  EXPECT_EQ(req, req_parsed);
  EXPECT_EQ(req_parsed.n_intervals(), mock_aux_value);
}

TEST(request_test, valid_compose_multiple) {
  using namespace std::string_literals;  // NOLINT
  static constexpr auto rq_type = request_type_code::intervals;
  static constexpr auto mock_aux_value = 1234;
  static constexpr auto mock_index_hash = 5678;
  const auto methylome_names = std::vector{
    "SRX012345"s,
    "asdf"s,
    "_V_A_P_O_R_"s,
  };
  request_buffer buf;
  const request req{rq_type, mock_index_hash, mock_aux_value, methylome_names};
  const std::error_code compose_ec = compose(buf, req);
  EXPECT_FALSE(compose_ec);
  request req_parsed;
  const std::error_code parse_ec = parse(buf, req_parsed);
  EXPECT_FALSE(parse_ec);
  EXPECT_EQ(req, req_parsed);
  EXPECT_EQ(std::size(req.methylome_names), std::size(methylome_names));
  EXPECT_EQ(req_parsed.n_intervals(), mock_aux_value);
}

TEST(request_test, compose_too_large) {
  using namespace std::string_literals;  // NOLINT
  static constexpr auto rq_type = request_type_code::intervals;
  static constexpr auto mock_aux_value = 1234;
  static constexpr auto mock_index_hash = 5678;
  static constexpr auto mock_methylome_name = "SRX012345"s;
  std::vector<std::string> methylome_names(60, mock_methylome_name);
  request_buffer buf;
  const request req{rq_type, mock_index_hash, mock_aux_value, methylome_names};
  const std::error_code compose_ec = compose(buf, req);
  EXPECT_EQ(compose_ec, request_error_code::request_too_large);
}

TEST(request_test, basic_assertions_bins) {
  const request req{request_type_code::bins, 0, 100, {"SRX12345"}};
  EXPECT_EQ(req.bin_size(), 100);
}
