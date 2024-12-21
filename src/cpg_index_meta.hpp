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

#ifndef SRC_CPG_INDEX_META_HPP_
#define SRC_CPG_INDEX_META_HPP_

#include <boost/describe.hpp>  // for BOOST_DESCRIBE_STRUCT

#include <cstdint>  // for uint32_t, int32_t
#include <format>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>  // for true_type
#include <unordered_map>
#include <utility>  // for to_underlying, unreachable
#include <variant>  // IWYU pragma: keep
#include <vector>

enum class cpg_index_meta_error : std::uint32_t {
  ok = 0,
  version_not_found = 1,
  host_not_found = 2,
  user_not_found = 3,
  creation_time_not_found = 4,
  chrom_names_not_found = 5,
  index_hash_not_found = 6,
  assembly_not_found = 7,
  n_cpgs_not_found = 8,
  failure_parsing_json = 9,
  inconsistent = 10,
  n_values = 11,
};

// register cpg_index_meta_error as error code enum
template <>
struct std::is_error_code_enum<cpg_index_meta_error> : public std::true_type {};

// category to provide text descriptions
struct cpg_index_meta_error_category : std::error_category {
  const char *
  name() const noexcept override {
    return "cpg_index_meta_error";
  }
  std::string
  message(int code) const override {
    using std::string_literals::operator""s;
    // clang-format off
    switch (code) {
    case 0: return "ok"s;
    case 1: return "verion not found"s;
    case 2: return "host not found"s;
    case 3: return "user not found"s;
    case 4: return "creation_time not found"s;
    case 5: return "chrom names not found"s;
    case 6: return "index_hash not found"s;
    case 7: return "assembly not found"s;
    case 8: return "n_cpgs not found"s;
    case 9: return "failure parsing methylome metadata json"s;
    case 10: return "inconsistent metadata"s;
    }
    // clang-format on
    std::unreachable();  // hopefully this is unreacheable
  }
};

inline std::error_code
make_error_code(cpg_index_meta_error e) {
  static auto category = cpg_index_meta_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

struct cpg_index_meta {
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

  auto
  operator<=>(const cpg_index_meta &other) const {
    // clang-format off
    if (version != other.version) return version <=> other.version;
    if (host != other.host) return host <=> other.host;
    if (user != other.user) return user <=> other.user;
    if (creation_time != other.creation_time) return creation_time <=> other.creation_time;
    if (index_hash != other.index_hash) return index_hash <=> other.index_hash;
    if (assembly != other.assembly) return assembly <=> other.assembly;
    if (n_cpgs != other.n_cpgs) return n_cpgs <=> other.n_cpgs;
    // ignoring the unordered_map chrom_index!
    if (chrom_order != other.chrom_order) return chrom_order <=> other.chrom_order;
    if (chrom_size != other.chrom_size) return chrom_size <=> other.chrom_size;
    return chrom_offset <=> other.chrom_offset;
    // clang-format on
  }

  [[nodiscard]] static auto
  read(const std::string &json_filename)
    -> std::tuple<cpg_index_meta, std::error_code>;

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
};

// clang-format off
BOOST_DESCRIBE_STRUCT(cpg_index_meta, (),
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

template <>
struct std::formatter<cpg_index_meta> : std::formatter<std::string> {
  auto
  format(const cpg_index_meta &cm, std::format_context &ctx) const {
    return std::formatter<std::string>::format(cm.tostring(), ctx);
  }
};

[[nodiscard]] auto
get_default_cpg_index_meta_filename(const std::string &indexfile)
  -> std::string;

[[nodiscard]] auto
get_assembly_from_filename(const std::string &filename,
                           std::error_code &ec) -> std::string;

#endif  // SRC_CPG_INDEX_META_HPP_
