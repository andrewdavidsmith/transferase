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

#ifndef SRC_METHYLOME_HPP_
#define SRC_METHYLOME_HPP_

#include "level_element.hpp"
#include "methylome_data.hpp"
#include "methylome_metadata.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>  // std::uint32_t
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace xfrase {

struct cpg_index;
struct query_container;

struct methylome {
  static constexpr auto data_extn = methylome_data::filename_extension;
  static constexpr auto meta_extn = methylome_metadata::filename_extension;

  methylome_data data;
  methylome_metadata meta;

  [[nodiscard]] static auto
  read(const std::string &dirname, const std::string &methylome_name,
       std::error_code &ec) -> methylome;

  [[nodiscard]] auto
  is_consistent() const -> bool {
    return meta.methylome_hash == data.hash();
  }

  [[nodiscard]] auto
  is_consistent(const methylome &other) const -> bool {
    return meta.is_consistent(other.meta);
  }

  [[nodiscard]] auto
  write(const std::string &outdir,
        const std::string &name) const -> std::error_code;

  [[nodiscard]] auto
  init_metadata(const cpg_index &index) -> std::error_code;

  [[nodiscard]] auto
  update_metadata() -> std::error_code;

  [[nodiscard]] auto
  tostring() const -> std::string {
    return meta.tostring();
  }

  auto
  add(const methylome &rhs) -> void {
    data.add(rhs.data);
  }

  /// get global methylation level
  [[nodiscard]] auto
  global_levels() const -> level_element_t {
    return data.global_levels();
  }

  /// get global methylation level and sites covered
  [[nodiscard]] auto
  global_levels_covered() const -> level_element_covered_t {
    return data.global_levels_covered();
  }

  /// get methylation levels for query intervals
  [[nodiscard]] auto
  get_levels(const xfrase::query_container &qry) const {
    return data.get_levels(qry);
  }

  /// get methylation levels for query intervals and number for query
  /// intervals covered
  [[nodiscard]] auto
  get_levels_covered(const xfrase::query_container &qry) const {
    return data.get_levels_covered(qry);
  }

  /// get methylation levels for bins
  [[nodiscard]] auto
  get_levels(const std::uint32_t bin_size, const cpg_index &index) const {
    return data.get_levels(bin_size, index);
  }

  /// get methylation levels for bins and number of bins covered
  [[nodiscard]] auto
  get_levels_covered(const std::uint32_t bin_size,
                     const cpg_index &index) const {
    return data.get_levels_covered(bin_size, index);
  }

  [[nodiscard]] static auto
  files_exist(const std::string &directory,
              const std::string &methylome_name) -> bool;

  [[nodiscard]] static auto
  list(const std::string &dirname,
       std::error_code &ec) -> std::vector<std::string>;

  [[nodiscard]] static auto
  parse_methylome_name(const std::string &filename) -> std::string;

  [[nodiscard]] static auto
  is_valid_name(const std::string &methylome_name) -> bool {
    return std::ranges::all_of(
      methylome_name, [](const auto c) { return std::isalnum(c) || c == '_'; });
  }
};

}  // namespace xfrase

// methylome error codes
enum class methylome_code : std::uint8_t {
  ok = 0,
  invalid_accession = 1,
  invalid_methylome_data = 2,
};

template <>
struct std::is_error_code_enum<methylome_code> : public std::true_type {};

struct methylome_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override { return "methylome"; }
  auto message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "invalid accession"s;
    case 2: return "invalid methylome data"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(methylome_code e) -> std::error_code {
  static auto category = methylome_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_METHYLOME_HPP_
