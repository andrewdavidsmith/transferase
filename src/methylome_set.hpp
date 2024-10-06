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

#ifndef SRC_METHYLOME_SET_HPP_
#define SRC_METHYLOME_SET_HPP_

#include "methylome.hpp"

#include <tuple>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdint>

template <typename T> struct ring_buffer {
  // queue ops and iterable
  ring_buffer(const std::size_t max_size) :
    max_size{max_size}, counter{0}, buf{max_size} {}
  auto push(T t) -> T {
    std::swap(buf[counter++ % max_size], t);
    return t;
  }
  auto size() const { return counter < max_size ? counter : max_size; }
  auto front() const {
    return buf[counter < max_size ? 0 : (counter + 1) % max_size];
  }

  auto begin() { return std::begin(buf); }
  auto begin() const { return std::cbegin(buf); }
  auto end() { return std::begin(buf) + std::min(counter, max_size); }
  auto end() const { return std::cbegin(buf) + std::min(counter, max_size); }

  // ADS: just use capacity within vector
  std::size_t max_size{};
  std::size_t counter{};
  std::vector<T> buf;
};

struct methylome_set {
  methylome_set(const methylome_set&) = delete;
  methylome_set& operator=(const methylome_set&) = delete;

  methylome_set(const bool verbose, const std::uint32_t max_live_methylomes,
                const std::string &mc16_directory)
      : verbose{verbose}, n_total_cpgs{},
        max_live_methylomes{max_live_methylomes},
        mc16_directory{mc16_directory}, accessions{max_live_methylomes} {}

  [[nodiscard]] auto
  get_methylome(const std::string &accession) -> std::tuple<const methylome *, int>;

  [[nodiscard]] auto
  summary() const -> std::string;

  [[nodiscard]] auto
  get_n_total_cpgs() const -> std::uint32_t {return n_total_cpgs;}

  static constexpr std::uint32_t default_max_live_methylomes{128};

  bool verbose{};
  std::uint32_t n_total_cpgs{};
  std::uint32_t max_live_methylomes{};
  std::string mc16_directory;
  ring_buffer<std::string> accessions;
  std::unordered_map<std::string, methylome> accession_to_methylome;
};

#endif  // SRC_METHYLOME_SET_HPP_
