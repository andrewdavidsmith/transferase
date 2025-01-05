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

#ifndef SRC_CPG_INDEX_METADATA_HPP_
#define SRC_CPG_INDEX_METADATA_HPP_

#include <boost/describe.hpp>  // for BOOST_DESCRIBE_STRUCT

#include <cstdint>  // for uint32_t, int32_t
#include <filesystem>
#include <format>
#include <string>
#include <system_error>
#include <type_traits>  // for true_type
#include <unordered_map>
#include <utility>  // for to_underlying, unreachable
#include <vector>

namespace xfrase {

struct cpg_index_metadata {
  static constexpr auto filename_extension{".cpg_idx.json"};

  std::string version;
  std::string host;
  std::string user;
  std::string creation_time;
  std::uint64_t index_hash{};
  std::string assembly;
  std::uint32_t n_cpgs{};
  std::unordered_map<std::string, std::int32_t> chrom_index;
  std::vector<std::string> chrom_order;
  std::vector<std::uint32_t> chrom_size;
  std::vector<std::uint32_t> chrom_offset;

  [[nodiscard]] static auto
  read(const std::string &json_filename,
       std::error_code &ec) -> cpg_index_metadata;

  [[nodiscard]] static auto
  read(const std::string &dirname, const std::string &genome_name,
       std::error_code &ec) -> cpg_index_metadata;

  [[nodiscard]] auto
  write(const std::string &json_filename) const -> std::error_code;

  [[nodiscard]] auto
  init_env() -> std::error_code;

  [[nodiscard]] auto
  tostring() const -> std::string;

  [[nodiscard]] auto
  get_n_cpgs_chrom() const -> std::vector<std::uint32_t>;

  [[nodiscard]] auto
  get_n_bins(const std::uint32_t bin_size) const -> std::uint32_t;

  [[nodiscard]] static auto
  compose_filename(auto wo_extension) {
    wo_extension += filename_extension;
    return wo_extension;
  }

  [[nodiscard]] static auto
  compose_filename(const auto &directory, const auto &name) {
    const auto wo_extn = (std::filesystem::path{directory} / name).string();
    return std::format("{}{}", wo_extn, filename_extension);
  }
};

// clang-format off
BOOST_DESCRIBE_STRUCT(cpg_index_metadata, (),
(
 version,
 host,
 user,
 creation_time,
 index_hash,
 assembly,
 n_cpgs,
 chrom_index,
 chrom_order,
 chrom_size,
 chrom_offset
))
// clang-format on

}  // namespace xfrase

template <>
struct std::formatter<xfrase::cpg_index_metadata>
  : std::formatter<std::string> {
  auto
  format(const xfrase::cpg_index_metadata &meta,
         std::format_context &ctx) const {
    return std::formatter<std::string>::format(meta.tostring(), ctx);
  }
};

enum class cpg_index_metadata_error : std::uint8_t {
  ok = 0,
  failure_parsing_json = 1,
};

template <>
struct std::is_error_code_enum<cpg_index_metadata_error>
  : public std::true_type {};

struct cpg_index_metadata_error_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override {
    return "cpg_index_metadata_error";
  }
  auto message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "failure parsing methylome metadata json"s;
    }
    std::unreachable();
  }
  // clang-format on
};

inline std::error_code
make_error_code(cpg_index_metadata_error e) {
  static auto category = cpg_index_metadata_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_CPG_INDEX_METADATA_HPP_
