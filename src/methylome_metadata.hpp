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

#ifndef SRC_METHYLOME_METADATA_HPP_
#define SRC_METHYLOME_METADATA_HPP_

#include <boost/describe.hpp>  // for BOOST_DESCRIBE_STRUCT

#include <cstdint>  // for uint32_t, uint64_t
#include <format>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>  // for std::true_type
#include <utility>      // for std::to_underlying, std::unreachable
#include <variant>      // for std::tuple

enum class methylome_metadata_error : std::uint32_t {
  ok = 0,
  version_not_found = 1,
  host_not_found = 2,
  user_not_found = 3,
  creation_time_not_found = 4,
  methylome_hash_not_found = 5,
  index_hash_not_found = 6,
  assembly_not_found = 7,
  n_cpgs_not_found = 8,
  failure_parsing_json = 9,
  inconsistent = 10,
};

// register methylome_metadata_error as error code enum
template <>
struct std::is_error_code_enum<methylome_metadata_error>
  : public std::true_type {};

// category to provide text descriptions
struct methylome_metadata_error_category : std::error_category {
  const char *
  name() const noexcept override {
    return "methylome_metadata_error";
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
    case 5: return "methylome_hash not found"s;
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
make_error_code(methylome_metadata_error e) {
  static auto category = methylome_metadata_error_category{};
  return std::error_code(std::to_underlying(e), category);
}

struct methylome;
struct cpg_index_metadata;

struct methylome_metadata {
  static constexpr auto filename_extension{".m16.json"};
  std::string version;
  std::string host;
  std::string user;
  std::string creation_time;
  std::uint64_t methylome_hash{};
  std::uint64_t index_hash{};
  std::string assembly;
  std::uint32_t n_cpgs{};
  bool is_compressed{};

  [[nodiscard]] static auto
  read(const std::string &json_filename)
    -> std::tuple<methylome_metadata, std::error_code>;

  [[nodiscard]] auto
  write(const std::string &json_filename) const -> std::error_code;

  [[nodiscard]] auto
  init_env() -> std::error_code;

  [[nodiscard]] auto
  tostring() const -> std::string;

  // ADS: (todo) think of a better way to get "compression" status
  [[nodiscard]] static auto
  init(const cpg_index_metadata &cim, const methylome &meth,
       const bool is_compressed)
    -> std::tuple<methylome_metadata, std::error_code>;

  [[nodiscard]] auto
  update(const methylome &meth) -> std::error_code;
};

// clang-format off
BOOST_DESCRIBE_STRUCT(methylome_metadata, (),
(
 version,
 host,
 user,
 creation_time,
 methylome_hash,
 index_hash,
 assembly,
 n_cpgs,
 is_compressed
))
// clang-format on

[[nodiscard]] inline auto
methylome_metadata_consistent(const methylome_metadata &a,
                              const methylome_metadata &b) -> bool {
  // clang-format off
  return (a.index_hash == b.index_hash ||
          a.n_cpgs == b.n_cpgs ||
          a.assembly == b.assembly ||
          a.version == b.version);
  // clang-format on
}

template <>
struct std::formatter<methylome_metadata> : std::formatter<std::string> {
  auto
  format(const methylome_metadata &mm, std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}", mm.tostring());
  }
};

auto
get_default_methylome_metadata_filename(const std::string &methfile)
  -> std::string;

#endif  // SRC_METHYLOME_METADATA_HPP_
