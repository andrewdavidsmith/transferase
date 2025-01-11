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

namespace transferase {

struct genome_index;
struct query_container;

struct methylome {
  static constexpr auto data_extn = methylome_data::filename_extension;
  static constexpr auto meta_extn = methylome_metadata::filename_extension;

  methylome_data data;
  methylome_metadata meta;

  methylome() = default;
  methylome(methylome_data &&data, methylome_metadata &&meta) :
    data{std::move(data)}, meta{std::move(meta)} {}

  [[nodiscard]] static auto
  read(const std::string &dirname, const std::string &methylome_name,
       std::error_code &ec) noexcept -> methylome;

#ifndef TRANSFERASE_NOEXCEPT
  static auto
  read(const std::string &dirname,
       const std::string &methylome_name) -> methylome {
    std::error_code ec;
    auto meth = read(dirname, methylome_name, ec);
    if (ec)
      throw std::system_error(ec);
    return meth;
  }
#endif

  [[nodiscard]] auto
  is_consistent() const noexcept -> bool {
    return meta.methylome_hash == data.hash();
  }

  [[nodiscard]] auto
  is_consistent(const methylome &other) const noexcept -> bool {
    return meta.is_consistent(other.meta);
  }

  [[nodiscard]]
  auto
  get_hash() const noexcept -> std::uint64_t {
    return meta.methylome_hash;
  }

  auto
  write(const std::string &outdir, const std::string &name,
        std::error_code &ec) const noexcept -> void;

#ifndef TRANSFERASE_NOEXCEPT
  auto
  write(const std::string &outdir, const std::string &name) const -> void {
    std::error_code ec;
    write(outdir, name, ec);
    if (ec)
      throw std::system_error(ec);
  }
#endif

  [[nodiscard]] auto
  init_metadata(const genome_index &index) noexcept -> std::error_code;

  [[nodiscard]] auto
  update_metadata() noexcept -> std::error_code;

  [[nodiscard]] auto
  tostring() const noexcept -> std::string {
    return meta.tostring();
  }

  auto
  add(const methylome &rhs) noexcept -> void {
    data.add(rhs.data);
  }

  /// get global methylation level
  [[nodiscard]] auto
  global_levels() const noexcept -> level_element_t {
    return data.global_levels();
  }

  /// get global methylation level and sites covered
  [[nodiscard]] auto
  global_levels_covered() const noexcept -> level_element_covered_t {
    return data.global_levels_covered();
  }

  /// get methylation levels for query intervals
  [[nodiscard]] auto
  get_levels(const transferase::query_container &query) const {
    return data.get_levels(query);
  }

  /// get methylation levels for query intervals and number for query
  /// intervals covered
  [[nodiscard]] auto
  get_levels_covered(const transferase::query_container &query) const {
    return data.get_levels_covered(query);
  }

  /// get methylation levels for bins
  [[nodiscard]] auto
  get_levels(const std::uint32_t bin_size, const genome_index &index) const {
    return data.get_levels(bin_size, index);
  }

  /// get methylation levels for bins and number of bins covered
  [[nodiscard]] auto
  get_levels_covered(const std::uint32_t bin_size,
                     const genome_index &index) const {
    return data.get_levels_covered(bin_size, index);
  }

  [[nodiscard]] static auto
  files_exist(const std::string &directory,
              const std::string &methylome_name) noexcept -> bool;

  [[nodiscard]] static auto
  list(const std::string &dirname,
       std::error_code &ec) noexcept -> std::vector<std::string>;

#ifndef TRANSFERASE_NOEXCEPT
  [[nodiscard]] static auto
  list(const std::string &dirname) -> std::vector<std::string> {
    std::error_code ec;
    auto methylome_names = list(dirname, ec);
    if (ec)
      throw std::system_error(ec);
    return methylome_names;
  }
#endif

  [[nodiscard]] static auto
  parse_methylome_name(const std::string &filename) noexcept -> std::string;

  [[nodiscard]] static auto
  is_valid_name(const std::string &methylome_name) noexcept -> bool {
    return std::ranges::all_of(
      methylome_name, [](const auto c) { return std::isalnum(c) || c == '_'; });
  }
};

}  // namespace transferase

// Specialization of std::hash for methylome
template <> struct std::hash<transferase::methylome> {
  auto
  operator()(const transferase::methylome &meth) const noexcept -> std::size_t {
    return meth.get_hash();
  }
};

// methylome error codes
enum class methylome_error_code : std::uint8_t {
  ok = 0,
  invalid_accession = 1,
  invalid_methylome_data = 2,
};

template <>
struct std::is_error_code_enum<methylome_error_code> : public std::true_type {};

struct methylome_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override { return "methylome"; }
  auto message(int code) const noexcept -> std::string override {
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
make_error_code(methylome_error_code e) noexcept -> std::error_code {
  static auto category = methylome_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_METHYLOME_HPP_
