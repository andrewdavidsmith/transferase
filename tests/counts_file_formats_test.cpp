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

#include <counts_file_formats.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

TEST(counts_file_format_test, parse_counts_line) {
  const std::string line{"chr1 202 + CG 0.963302752293578 109"};
  std::uint32_t pos{}, n_meth{}, n_unmeth{};
  bool parse_success = parse_counts_line(line, pos, n_meth, n_unmeth);
  EXPECT_TRUE(parse_success)
    << "failed parse_success for: \"" << line << "\"\n";
  EXPECT_EQ(pos, 202) << "failed pos for: \"" << line << "\"\n";
  EXPECT_EQ(n_meth, 105) << "failed n_meth for: \"" << line << "\"\n";
  EXPECT_EQ(n_unmeth, 4) << "failed n_unmeth for: \"" << line << "\"\n";

  const std::string line2{"chr1 22736 + CG 0.050505050505050504 99"};
  parse_success = parse_counts_line(line2, pos, n_meth, n_unmeth);
  EXPECT_TRUE(parse_success)
    << "failed parse_success for: \"" << line2 << "\"\n";
  EXPECT_EQ(pos, 22736) << "failed pos for: \"" << line2 << "\"\n";
  EXPECT_EQ(n_meth, 5) << "failed n_meth for: \"" << line2 << "\"\n";
  EXPECT_EQ(n_unmeth, 94) << "failed n_unmeth for: \"" << line2 << "\"\n";

  const std::string line3{"chr7\t22858\t+\tCG\t0.07954545454545454\t88"};
  parse_success = parse_counts_line(line3, pos, n_meth, n_unmeth);
  EXPECT_TRUE(parse_success)
    << "failed parse_success for: \"" << line3 << "\"\n";
  EXPECT_EQ(pos, 22858) << "failed pos for: \"" << line3 << "\"\n";
  EXPECT_EQ(n_meth, 7) << "failed n_meth for: \"" << line3 << "\"\n";
  EXPECT_EQ(n_unmeth, 81) << "failed n_unmeth for: \"" << line3 << "\"\n";

  const std::string line4{"chr1 10576 + CpG 0.333333 3"};
  parse_success = parse_counts_line(line4, pos, n_meth, n_unmeth);
  EXPECT_TRUE(parse_success)
    << "failed parse_success for: \"" << line4 << "\"\n";
  EXPECT_EQ(pos, 10576) << "failed pos for: \"" << line4 << "\"\n";
  EXPECT_EQ(n_meth, 1) << "failed n_meth for: \"" << line4 << "\"\n";
  EXPECT_EQ(n_unmeth, 2) << "failed n_unmeth for: \"" << line4 << "\"\n";
}
