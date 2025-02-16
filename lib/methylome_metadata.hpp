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

#ifndef LIB_METHYLOME_METADATA_HPP_
#define LIB_METHYLOME_METADATA_HPP_

#include "nlohmann/json.hpp"

#include <boost/describe.hpp>  // for BOOST_DESCRIBE_STRUCT

#include <cstdint>  // for uint32_t, uint64_t
#include <filesystem>
#include <format>
#include <string>
#include <system_error>

namespace transferase {

struct methylome_metadata {
  static constexpr auto filename_extension{".m16.json"};
  std::string version;
  std::string host;
  std::string user;
  std::string creation_time;
  std::uint64_t methylome_hash{};
  std::uint64_t index_hash{};
  std::string genome_name;
  std::uint32_t n_cpgs{};
  bool is_compressed{};

  [[nodiscard]] auto
  is_valid() const -> bool {
    // clang-format off
    return (!version.empty()       &&
            !host.empty()          &&
            !user.empty()          &&
            !creation_time.empty() &&
            !genome_name.empty());
    // clang-format on
  }

  [[nodiscard]] auto
  is_consistent(const methylome_metadata &rhs) const -> bool {
    // clang-format off
    return (index_hash  == rhs.index_hash  &&
            n_cpgs      == rhs.n_cpgs      &&
            genome_name == rhs.genome_name &&
            version     == rhs.version);
    // clang-format on
  }

  [[nodiscard]] static auto
  read(const std::string &json_filename,
       std::error_code &ec) -> methylome_metadata;

  [[nodiscard]] static auto
  read(const std::string &dirname, const std::string &methylome_name,
       std::error_code &ec) -> methylome_metadata;

  [[nodiscard]] auto
  write(const std::string &json_filename) const -> std::error_code;

  [[nodiscard]] auto
  init_env() -> std::error_code;

  [[nodiscard]] auto
  tostring() const -> std::string;

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

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(methylome_metadata, version, host, user,
                                 creation_time, methylome_hash, index_hash,
                                 genome_name, n_cpgs, is_compressed)
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
 genome_name,
 n_cpgs,
 is_compressed
))
// clang-format on

}  // namespace transferase

template <>
struct std::formatter<transferase::methylome_metadata>
  : std::formatter<std::string> {
  auto
  format(const transferase::methylome_metadata &mm,
         std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}", mm.tostring());
  }
};

#endif  // LIB_METHYLOME_METADATA_HPP_
