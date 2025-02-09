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

#ifndef SRC_GENOME_INDEX_SET_HPP_
#define SRC_GENOME_INDEX_SET_HPP_

#include "ring_buffer.hpp"

#include <cstddef>  // for std::size_t
#include <cstdint>  // for std::uint32_t
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <unordered_map>
#include <utility>  // for std::to_underlying, std::unreachable

namespace transferase {

struct genome_index;

struct genome_index_set {
  // prevent copy; move disallowed because of std::mutex member
  genome_index_set(const genome_index_set &) = delete;
  genome_index_set &
  operator=(const genome_index_set &) = delete;

  explicit genome_index_set(const std::string &genome_index_directory,
                            const std::uint32_t max_live_genome_indexes =
                              default_max_live_genome_indexes) :
    genome_index_directory{genome_index_directory},
    max_live_genome_indexes{max_live_genome_indexes},
    genome_names{max_live_genome_indexes} {}

  [[nodiscard]] auto
  get_genome_index(const std::string &genome_name,
                   std::error_code &ec) -> std::shared_ptr<genome_index>;

  [[nodiscard]] auto
  size() const noexcept -> std::size_t {
    return std::size(name_to_index);
  }

  static constexpr std::uint32_t default_max_live_genome_indexes{8};

  std::mutex mtx;
  std::string genome_index_directory;
  std::uint32_t max_live_genome_indexes{};

  ring_buffer<std::string> genome_names;
  std::unordered_map<std::string, std::shared_ptr<genome_index>> name_to_index;
};

}  // namespace transferase

// error code for genome_index_set
enum class genome_index_set_error_code : std::uint8_t {
  ok = 0,
  error_loading_genome_index = 1,
  genome_index_not_found = 2,
  unknown_error = 3,
};

template <>
struct std::is_error_code_enum<genome_index_set_error_code>
  : public std::true_type {};

struct genome_index_set_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override {return "genome_index_set";}
  auto message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "error loading genome index"s;
    case 2: return "genome index not found"s;
    case 3: return "genome index unknown error"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(genome_index_set_error_code e) -> std::error_code {
  static auto category = genome_index_set_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_GENOME_INDEX_SET_HPP_
