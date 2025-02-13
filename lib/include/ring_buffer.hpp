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

#ifndef LIB_INCLUDE_RING_BUFFER_HPP_
#define LIB_INCLUDE_RING_BUFFER_HPP_

#include <algorithm>
#include <cstddef>  // for std::size_t
#include <iterator>
#include <vector>

namespace transferase {

template <typename T> struct ring_buffer {
  // support queue ops and be iterable
  explicit ring_buffer(const std::size_t capacity) :
    capacity{capacity}, counter{0}, buf(capacity) {}
  // clang-format off
  auto push_back(const T &t) {buf[counter++ % capacity] = t;}
  [[nodiscard]] auto
  size() const {return counter < capacity ? counter : capacity;}
  [[nodiscard]] auto
  full() const {return counter >= capacity;}
  [[nodiscard]] auto
  front() const {return buf[counter < capacity ? 0 : counter % capacity];}
  [[nodiscard]] auto
  begin() {return std::begin(buf);}
  [[nodiscard]] auto
  begin() const {return std::cbegin(buf);}
  [[nodiscard]] auto
  end() {return std::begin(buf) + std::min(counter, capacity);}
  [[nodiscard]] auto
  end() const {return std::cbegin(buf) + std::min(counter, capacity);}
  // clang-format on

  // ADS: just use capacity inside the vector?
  std::size_t capacity{};
  std::size_t counter{};
  std::vector<T> buf;
};

}  // namespace transferase

#endif  // LIB_INCLUDE_RING_BUFFER_HPP_
