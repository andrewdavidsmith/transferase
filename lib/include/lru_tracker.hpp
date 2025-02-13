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

#ifndef LIB_INCLUDE_LRU_TRACKER_HPP_
#define LIB_INCLUDE_LRU_TRACKER_HPP_

#include <cassert>
#include <cstddef>
#include <format>
#include <initializer_list>  // for std::begin
#include <iterator>          // for std::cend, std::size
#include <list>
#include <string>
#include <unordered_map>

namespace transferase {

template <typename T> class lru_tracker {
public:
  explicit lru_tracker(const std::size_t capacity) : capacity{capacity} {
    assert(capacity > 0);
  }

  [[nodiscard]] auto
  size() const noexcept -> std::size_t {
    return std::size(the_map);
  }

  [[nodiscard]] auto
  full() const noexcept -> bool {
    return size() >= capacity;
  }

  [[nodiscard]] auto
  back() const noexcept -> const T & {
    return the_list.back();
  }

  [[nodiscard]] auto
  string() const noexcept -> std::string {
    std::string r;
    for (const auto &elem : the_list) {
      const auto itr = the_map.find(elem);
      const auto addr = reinterpret_cast<std::size_t>(&(*itr));  // NOLINT
      r += std::format("{}\t{}\n", elem, addr);
    }
    return r;
  }

  auto
  move_to_front(const T &s) noexcept {
    const auto itr = the_map.find(s);
    assert(itr != std::cend(the_map));

    //// ADS: benchmark
    // T tmp = std::move(*itr->second);
    // the_list.erase(itr->second);
    // the_list.emplace_front(std::move(tmp));

    // delete the element from the list
    the_list.erase(itr->second);
    the_list.push_front(s);
    itr->second = std::begin(the_list);
  }

  auto
  pop() noexcept {
    const T &s = the_list.back();
    the_map.erase(s);
    the_list.pop_back();
  }

  auto
  push(const T &s) noexcept {
    if (full())
      pop();
    the_list.push_front(s);
    the_map.emplace(s, std::begin(the_list));
  }

private:
  std::list<T> the_list;
  std::unordered_map<T, typename std::list<T>::iterator> the_map;
  std::size_t capacity{};
};

}  // namespace transferase

#endif  // LIB_INCLUDE_LRU_TRACKER_HPP_
