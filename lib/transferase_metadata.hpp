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

#ifndef LIB_TRANSFERASE_METADATA_HPP_
#define LIB_TRANSFERASE_METADATA_HPP_

#include "nlohmann/json.hpp"

#include <cstdint>
#include <ranges>
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <unordered_map>
#include <utility>  // for std::to_underlying, std::unreachable
#include <vector>

namespace transferase {

struct transferase_metadata {
  std::unordered_map<std::string, std::vector<std::string>>
    genome_to_methylomes;
  std::unordered_map<std::string, std::string> methylome_to_genome;

  [[nodiscard]] auto
  get_genome(const std::vector<std::string> &methylome_names,
             std::error_code &error) const noexcept -> std::string;

  [[nodiscard]] static auto
  read(const std::string &json_filename,
       std::error_code &error) noexcept -> transferase_metadata;

  [[nodiscard]] auto
  available_genomes() const noexcept -> std::vector<std::string> {
    return genome_to_methylomes | std::views::elements<0> |
           std::ranges::to<std::vector<std::string>>();
  }

  [[nodiscard]] auto
  tostring() const -> std::string;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(transferase_metadata, genome_to_methylomes,
                                 methylome_to_genome)
};

}  // namespace transferase

/// @brief Enum for error codes related to transferase_metadata
enum class transferase_metadata_error_code : std::uint8_t {
  ok = 0,
  error_reading_transferase_metadata_json_file = 1,
  error_parsing_transferase_metadata_json_file = 2,
  methylome_not_found_in_metadata = 3,
};

template <>
struct std::is_error_code_enum<transferase_metadata_error_code>
  : public std::true_type {};

struct transferase_metadata_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override { return "transferase_metadata"; }
  auto message(int code) const noexcept -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "error reading transferase metadata json file"s;
    case 2: return "error parsing transferase metadata json file"s;
    case 3: return "methylome not found in metadata"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(transferase_metadata_error_code e) noexcept -> std::error_code {
  static auto category = transferase_metadata_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // LIB_TRANSFERASE_METADATA_HPP_
