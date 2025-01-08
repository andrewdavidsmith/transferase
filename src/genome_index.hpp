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

#ifndef SRC_GENOME_INDEX_HPP_
#define SRC_GENOME_INDEX_HPP_

#include "genome_index_data.hpp"
#include "genome_index_metadata.hpp"
#include "query_container.hpp"

#include <algorithm>
#include <cctype>   // for std::isalnum
#include <cstdint>  // for std::uint32_t
#include <format>   // for std::vector(??)
#include <ranges>   // IWYU pragma: keep
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <utility>      // for std::to_underlying, std::unreachable
#include <vector>

namespace transferase {

struct genomic_interval;

struct genome_index {
  static constexpr auto data_extn = genome_index_data::filename_extension;
  static constexpr auto meta_extn = genome_index_metadata::filename_extension;

  genome_index_data data;
  genome_index_metadata meta;

  genome_index() = default;
  genome_index(genome_index_data &&data, genome_index_metadata &&meta) :
    data{std::move(data)}, meta{std::move(meta)} {}

  // prevent copy, allow move
  // clang-format off
  genome_index(const genome_index &) = delete;
  genome_index &operator=(const genome_index &) = delete;
  genome_index(genome_index &&) noexcept = default;
  genome_index &operator=(genome_index &&) noexcept = default;
  // clang-format on

  [[nodiscard]] auto
  tostring() const -> std::string {
    return std::format(R"json({{"meta"={}, "data"={}}})json", meta, data);
  }

  [[nodiscard]] static auto
  read(const std::string &dirname, const std::string &genome_name,
       std::error_code &ec) -> genome_index;

  [[nodiscard]]
  auto
  is_consistent() const -> bool {
    return meta.index_hash == data.hash();
  }

  [[nodiscard]]
  auto
  get_hash() const -> std::uint64_t {
    return meta.index_hash;
  }

  [[nodiscard]] auto
  write(const std::string &outdir,
        const std::string &name) const -> std::error_code;

  [[nodiscard]] auto
  make_query(const std::vector<genomic_interval> &gis) const
    -> transferase::query_container;

  [[nodiscard]] auto
  get_n_cpgs_chrom() const {
    return meta.get_n_cpgs_chrom();
  }

  [[nodiscard]] auto
  get_n_bins(const std::uint32_t bin_size) const {
    return meta.get_n_bins(bin_size);
  }

  [[nodiscard]] static auto
  make_genome_index(const std::string &genome_file,
                    std::error_code &ec) -> genome_index;

  [[nodiscard]] static auto
  files_exist(const std::string &directory,
              const std::string &genome_name) -> bool;

  [[nodiscard]] static auto
  parse_genome_name(const std::string &filename,
                    std::error_code &ec) -> std::string;

  [[nodiscard]] static auto
  is_valid_name(const std::string &genome_name) -> bool {
    return std::ranges::all_of(
      genome_name, [](const auto c) { return std::isalnum(c) || c == '_'; });
  }

  [[nodiscard]] static auto
  list_genome_indexes(const std::string &dirname,
                      std::error_code &ec) -> std::vector<std::string>;
};

}  // namespace transferase

// genome_index errors

enum class genome_index_error_code : std::uint8_t {
  ok = 0,
  invalid_genome_name = 1,
  failure_processing_fasta_file = 2,
};

template <>
struct std::is_error_code_enum<genome_index_error_code>
  : public std::true_type {};

struct genome_index_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override {return "genome_index";}
  auto message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "invalid genome name"s;
    case 2: return "failure processing FASTA file"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(genome_index_error_code e) -> std::error_code {
  static auto category = genome_index_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_GENOME_INDEX_HPP_
