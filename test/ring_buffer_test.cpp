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

#include <ring_buffer.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace transferase;  // NOLINT

TEST(ring_buffer_test, push_back_and_size) {
  ring_buffer<std::string> buffer(3);
  EXPECT_EQ(buffer.size(), 0);
  buffer.push_back("one");
  EXPECT_EQ(buffer.size(), 1);
  buffer.push_back("two");
  EXPECT_EQ(buffer.size(), 2);
  buffer.push_back("three");
  EXPECT_EQ(buffer.size(), 3);
  buffer.push_back("four");  // This should overwrite the first element
  EXPECT_EQ(buffer.size(), 3);
}

TEST(ring_buffer_test, full) {
  ring_buffer<std::string> buffer(3);
  EXPECT_FALSE(buffer.full());
  buffer.push_back("one");
  buffer.push_back("two");
  buffer.push_back("three");
  EXPECT_TRUE(buffer.full());
  buffer.push_back("four");
  EXPECT_TRUE(buffer.full());
}

TEST(ring_buffer_test, front) {
  ring_buffer<std::string> buffer(3);
  buffer.push_back("one");
  buffer.push_back("two");
  buffer.push_back("three");
  EXPECT_EQ(buffer.front(), "one");
  buffer.push_back("four");  // This should overwrite the first element
  EXPECT_EQ(buffer.front(), "two");
}

TEST(ring_buffer_test, begin_and_end) {
  ring_buffer<std::string> buffer(3);
  buffer.push_back("one");
  buffer.push_back("two");
  buffer.push_back("three");
  auto it = buffer.begin();
  EXPECT_EQ(*it, "one");
  ++it;
  EXPECT_EQ(*it, "two");
  ++it;
  EXPECT_EQ(*it, "three");
  EXPECT_EQ(it + 1, buffer.end());
}
