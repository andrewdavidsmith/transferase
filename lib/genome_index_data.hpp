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

#ifndef LIB_GENOME_INDEX_DATA_HPP_
#define LIB_GENOME_INDEX_DATA_HPP_

#include "aligned_allocator.hpp"
#include "query_container.hpp"

#include <cstddef>  // for std::size_t
#include <cstdint>  // for std::uint32_t, std::int32_t, std::uint64_t
#include <filesystem>
#include <format>
#include <initializer_list>
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <utility>      // for std::pair
#include <variant>      // for std::hash
#include <vector>

namespace transferase {

struct genomic_interval;
struct genome_index_metadata;
struct chrom_range_t;

struct genome_index_data {
  // includes the dot because that's how std::filesystem::path works
  static constexpr auto filename_extension{".cpg_idx"};

  typedef std::uint32_t genome_pos_t;
  typedef std::vector<genome_pos_t, aligned_allocator<genome_pos_t>> vec;

  explicit genome_index_data(
    const std::initializer_list<genome_index_data::vec> &l) : positions(l) {}

  // prevent copying and allow moving
  // clang-format off
  genome_index_data() = default;
  genome_index_data(const genome_index_data &) = delete;
  genome_index_data &operator=(const genome_index_data &) = delete;
  genome_index_data(genome_index_data &&) noexcept = default;
  genome_index_data &operator=(genome_index_data &&) noexcept = default;
  // clang-format on

  [[nodiscard]] auto
  tostring() const noexcept -> std::string {
    return std::format(R"json({{"size": {}}})json", get_n_cpgs());
  }

  [[nodiscard]] static auto
  read(const std::string &filename, const genome_index_metadata &meta,
       std::error_code &ec) noexcept -> genome_index_data;

  [[nodiscard]] static auto
  read(const std::string &dirname, const std::string &genomic_name,
       const genome_index_metadata &meta,
       std::error_code &ec) noexcept -> genome_index_data;

  [[nodiscard]] auto
  write(const std::string &index_file) const noexcept -> std::error_code;

  [[nodiscard]] auto
  hash() const noexcept -> std::uint64_t;

  [[nodiscard]] auto
  get_n_cpgs() const noexcept -> std::uint32_t;

  [[nodiscard]] auto
  make_query_chrom(const std::int32_t ch_id, const genome_index_metadata &meta,
                   const std::vector<chrom_range_t> &pos) const noexcept
    -> transferase::query_container;

  [[nodiscard]] auto
  make_query(const genome_index_metadata &meta,
             const std::vector<genomic_interval> &gis) const noexcept
    -> transferase::query_container;

  [[nodiscard]] static auto
  compose_filename(auto originally_without_extension) {
    originally_without_extension += filename_extension;
    return originally_without_extension;
  }

  [[nodiscard]] static auto
  compose_filename(const auto &directory, const auto &name) {
    const auto withou_ext = (std::filesystem::path{directory} / name).string();
    return std::format("{}{}", withou_ext, filename_extension);
  }

  std::vector<genome_index_data::vec> positions;
};

}  // namespace transferase

/// Specialization of std::hash for genome_index_data
template <> struct std::hash<transferase::genome_index_data> {
  auto
  operator()(const transferase::genome_index_data &data) const noexcept
    -> std::size_t {
    return data.hash();
  }
};

/// Specialization of std::format for genome_index_data
template <>
struct std::formatter<transferase::genome_index_data>
  : std::formatter<std::string> {
  auto
  format(const transferase::genome_index_data &data,
         std::format_context &ctx) const noexcept {
    return std::format_to(ctx.out(), "{}", data.tostring());
  }
};

// genome_index_data errors
enum class genome_index_data_error_code : std::uint8_t {
  ok = 0,
  failure_reading_file = 1,
};

template <>
struct std::is_error_code_enum<genome_index_data_error_code>
  : public std::true_type {};

struct genome_index_data_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override {return "genome_index_data";}
  auto message(int code) const noexcept -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "failure reading file"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(genome_index_data_error_code e) noexcept -> std::error_code {
  static auto category = genome_index_data_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // LIB_GENOME_INDEX_DATA_HPP_
