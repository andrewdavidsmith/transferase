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

#ifndef SRC_CPG_INDEX_DATA_HPP_
#define SRC_CPG_INDEX_DATA_HPP_

#if not defined(__APPLE__) && not defined(__MACH__)
#include "aligned_allocator.hpp"  // for aligned_allocator
#endif

#include <cstdint>  // for std::uint32_t, std::int32_t, std::uint64_t
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>  // for std::true_type
#include <utility>      // for std::pair
#include <vector>

struct genomic_interval;
struct cpg_index_metadata;

struct cpg_index_data {
  // includes the dot because that's how std::filesystem::path works
  static constexpr auto filename_extension{".cpg_idx"};

  typedef std::uint32_t cpg_pos_t;
#if not defined(__APPLE__) && not defined(__MACH__)
  typedef std::vector<cpg_pos_t, aligned_allocator<cpg_pos_t>> vec;
#else
  typedef std::vector<cpg_pos_t> vec;
#endif

  [[nodiscard]] static auto
  read(const std::string &index_file, const cpg_index_metadata &cim)
    -> std::tuple<cpg_index_data, std::error_code>;
  [[nodiscard]] auto
  write(const std::string &index_file) const -> std::error_code;

  [[nodiscard]] auto
  hash() const -> std::uint64_t;

  [[nodiscard]] auto
  get_n_cpgs() const -> std::uint32_t;

  [[nodiscard]] auto
  get_offsets_within_chrom(
    const std::int32_t ch_id,
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> &pos) const
    -> std::vector<std::pair<std::uint32_t, std::uint32_t>>;

  [[nodiscard]] auto
  get_offsets(const std::int32_t ch_id, const cpg_index_metadata &cim,
              const std::vector<std::pair<std::uint32_t, std::uint32_t>> &pos)
    const -> std::vector<std::pair<std::uint32_t, std::uint32_t>>;

  [[nodiscard]] auto
  get_offsets(const cpg_index_metadata &cim,
              const std::vector<genomic_interval> &gis) const
    -> std::vector<std::pair<std::uint32_t, std::uint32_t>>;

  std::vector<cpg_index_data::vec> positions;
};

[[nodiscard]] inline auto
compose_cpg_index_data_filename(auto wo_extension) {
  wo_extension += cpg_index_data::filename_extension;
  return wo_extension;
}

[[nodiscard]] auto
initialize_cpg_index_data(const std::string &genome_file)
  -> std::tuple<cpg_index_data, cpg_index_metadata, std::error_code>;

// cpg_index_data errors

enum class cpg_index_data_code : std::uint32_t {
  ok = 0,
  failure_reading_index_data = 1,
};

// register cpg_index_data_code as error code enum
template <>
struct std::is_error_code_enum<cpg_index_data_code> : public std::true_type {};

// category to provide text descriptions
struct cpg_index_data_category : std::error_category {
  auto
  name() const noexcept -> const char * override {
    return "cpg_index_data";
  }
  auto
  message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    // clang-format off
    switch (code) {
    case 0: return "ok"s;
    case 1: return "failure reading index data"s;
    }
    // clang-format on
    std::unreachable();  // hopefully
  }
};

inline auto
make_error_code(cpg_index_data_code e) -> std::error_code {
  static auto category = cpg_index_data_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_CPG_INDEX_DATA_HPP_
