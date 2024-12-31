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

#ifndef SRC_CPG_INDEX_HPP_
#define SRC_CPG_INDEX_HPP_

#include "cpg_index_data.hpp"
#include "cpg_index_metadata.hpp"
#include "query.hpp"

#include <cstdint>  // for std::uint32_t
#include <format>   // for std::vector???
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <utility>      // for std::to_underlying, std::unreachable
#include <vector>

namespace xfrase {

struct genomic_interval;

struct cpg_index {
  static constexpr auto data_extn = cpg_index_data::filename_extension;
  static constexpr auto meta_extn = cpg_index_metadata::filename_extension;

  cpg_index_data data;
  cpg_index_metadata meta;

  [[nodiscard]] static auto
  read(const std::string &dirname, const std::string &genome_name,
       std::error_code &ec) -> cpg_index;

  [[nodiscard]]
  auto
  is_consistent() const -> bool;

  [[nodiscard]]
  auto
  get_hash() const -> std::uint64_t {
    return meta.index_hash;
  }

  [[nodiscard]] auto
  write(const std::string &outdir,
        const std::string &name) const -> std::error_code;

  [[nodiscard]] auto
  make_query(const std::vector<genomic_interval> &gis) const -> xfrase::query;
};

[[nodiscard]] auto
make_cpg_index(const std::string &genome_file,
               std::error_code &ec) -> cpg_index;

[[nodiscard]] auto
cpg_index_files_exist(const std::string &directory,
                      const std::string &cpg_index_name) -> bool;

[[nodiscard]] auto
get_assembly_from_filename(const std::string &filename,
                           std::error_code &ec) -> std::string;

[[nodiscard]] auto
list_cpg_indexes(const std::string &dirname,
                 std::error_code &ec) -> std::vector<std::string>;

}  // namespace xfrase

// cpg_index errors

enum class cpg_index_code : std::uint8_t {
  ok = 0,
  wrong_identifier_in_header = 1,
  error_parsing_index_header_line = 2,
  failure_reading_index_header = 3,
  failure_reading_index_body = 4,
  inconsistent_chromosome_sizes = 5,
  failure_processing_genome_file = 6,
};

template <>
struct std::is_error_code_enum<cpg_index_code> : public std::true_type {};

struct cpg_index_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override {return "cpg_index";}
  auto message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "wrong identifier in header"s;
    case 2: return "error parsing index header line"s;
    case 3: return "failure reading index header"s;
    case 4: return "failure reading index body"s;
    case 5: return "inconsistent chromosome sizes"s;
    case 6: return "failure processing genome file"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline auto
make_error_code(cpg_index_code e) -> std::error_code {
  static auto category = cpg_index_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_CPG_INDEX_HPP_
