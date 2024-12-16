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

#ifndef SRC_ALIGNED_ALLOCATOR_HPP_
#define SRC_ALIGNED_ALLOCATOR_HPP_

#include <cstdlib>
#include <limits>
#include <new>
#include <numeric>

template <class T> struct aligned_allocator {
  static constexpr std::size_t align_at = 4096;
  typedef T value_type;
  [[nodiscard]] T *
  allocate(const std::size_t n) {
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
      throw std::bad_array_new_length();
    const auto requested = n * sizeof(T);
    const auto to_allocate = ((requested + align_at - 1) / align_at) * align_at;
    if (auto p = static_cast<T *>(std::aligned_alloc(align_at, to_allocate)))
      return p;
    throw std::bad_alloc();
  }
  void
  deallocate(T *p, [[maybe_unused]] const std::size_t n) noexcept {
    std::free(p);
  }
};

#endif  // SRC_ALIGNED_ALLOCATOR_HPP_
