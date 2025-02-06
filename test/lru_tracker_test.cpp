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

#include <lru_tracker.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace transferase;  // NOLINT

TEST(lru_tracker_test, push_and_size) {
  lru_tracker<std::string> tracker(3);
  EXPECT_EQ(std::size(tracker), 0);
  tracker.push("one");
  EXPECT_EQ(std::size(tracker), 1);
  tracker.push("two");
  EXPECT_EQ(std::size(tracker), 2);
  tracker.push("three");
  EXPECT_EQ(std::size(tracker), 3);
  tracker.push("four");  // This should overwrite the first element
  EXPECT_EQ(std::size(tracker), 3);
}

TEST(lru_tracker_test, full) {
  lru_tracker<std::string> tracker(3);
  EXPECT_FALSE(tracker.full());
  tracker.push("one");
  tracker.push("two");
  tracker.push("three");
  EXPECT_TRUE(tracker.full());
  tracker.push("four");
  EXPECT_TRUE(tracker.full());
}

TEST(lru_tracker_test, front) {
  lru_tracker<std::string> tracker(3);
  tracker.push("one");
  tracker.push("two");
  tracker.push("three");
  EXPECT_EQ(tracker.back(), "one");
  tracker.push("four");  // This should overwrite the first element
  EXPECT_EQ(tracker.back(), "two");
}

TEST(lru_tracker_test, move_to_front) {
  lru_tracker<std::string> tracker(4);
  tracker.push("one");
  tracker.push("two");
  tracker.push("three");
  tracker.push("four");
  EXPECT_EQ(tracker.back(), "one");
  tracker.move_to_front("one");  // This should overwrite the first element
  EXPECT_EQ(tracker.back(), "two");
}
