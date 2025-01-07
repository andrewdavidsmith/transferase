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

#ifndef SRC_GENOME_INDEX_SET_HPP_
#define SRC_GENOME_INDEX_SET_HPP_

#include <cstdint>  // for std::uint32_t
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <unordered_map>
#include <utility>  // for std::to_underlying, std::unreachable

namespace transferase {

struct genome_index;

struct genome_index_set {
  genome_index_set(const genome_index_set &) = delete;
  genome_index_set &
  operator=(const genome_index_set &) = delete;

  // ADS: this genome_index_set constructor always attempts to read files
  // so the error code is needed here; this contrasts with
  // methylome_set, which does no such work until requested.
  genome_index_set(const std::string &genome_index_directory,
                   std::error_code &ec);

  [[nodiscard]] auto
  get_genome_index(const std::string &assembly,
                   std::error_code &ec) -> std::shared_ptr<genome_index>;

  std::unordered_map<std::string, std::shared_ptr<genome_index>>
    assembly_to_genome_index;
};

}  // namespace transferase

// error code for genome_index_set
enum class genome_index_set_error : std::uint8_t {
  ok = 0,
  genome_index_not_found = 1,
};

template <>
struct std::is_error_code_enum<genome_index_set_error> : public std::true_type {
};

struct genome_index_set_category : std::error_category {
  auto
  name() const noexcept -> const char * override {
    return "genome_index_set";
  }
  auto
  message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    // clang-format off
    switch (code) {
    case 0: return "ok"s;
    case 1: return "cpg index not found"s;
    }
    // clang-format on
    std::unreachable();  // hopefully
  }
};

inline auto
make_error_code(genome_index_set_error e) -> std::error_code {
  static auto category = genome_index_set_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_GENOME_INDEX_SET_HPP_
