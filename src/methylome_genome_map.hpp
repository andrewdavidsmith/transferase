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

#ifndef SRC_METHYLOME_GENOME_MAP_HPP_
#define SRC_METHYLOME_GENOME_MAP_HPP_

#include <boost/describe.hpp>

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <unordered_map>
#include <utility>  // for std::to_underlying, std::unreachable
#include <vector>

namespace transferase {

struct methylome_genome_map {
  std::unordered_map<std::string, std::vector<std::string>>
    genome_to_methylomes;
  std::unordered_map<std::string, std::string> methylome_to_genome;

  [[nodiscard]] auto
  get_genome(const std::vector<std::string> &methylome_names,
             std::error_code &error) const noexcept -> std::string;

  [[nodiscard]] static auto
  read(const std::string &json_filename,
       std::error_code &error) noexcept -> methylome_genome_map;

  [[nodiscard]] auto
  string() const noexcept -> std::string;
};

// clang-format off
BOOST_DESCRIBE_STRUCT(methylome_genome_map, (),
(
 genome_to_methylomes,
 methylome_to_genome
))
// clang-format on

}  // namespace transferase

/// @brief Enum for error codes related to methylome_genome_map
enum class methylome_genome_map_error_code : std::uint8_t {
  ok = 0,
  error_reading_metadata_json_file = 1,
  error_parsing_metadata_json_file = 2,
};

template <>
struct std::is_error_code_enum<methylome_genome_map_error_code>
  : public std::true_type {};

struct methylome_genome_map_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override { return "methylome_genome_map"; }
  auto message(int code) const noexcept -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "error reading metadata json file"s;
    case 2: return "error parsing metadata json file"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(methylome_genome_map_error_code e) noexcept -> std::error_code {
  static auto category = methylome_genome_map_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_METHYLOME_GENOME_MAP_HPP_
