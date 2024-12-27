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

#include "methylome_data.hpp"
#include "methylome_metadata.hpp"

#include <cstddef>  // for std::size_t
#include <format>   // for std::vector??
#include <string>
#include <system_error>
#include <vector>

struct methylome {
  static constexpr auto data_extn = methylome_data::filename_extension;
  static constexpr auto meta_extn = methylome_metadata::filename_extension;

  methylome_data data;
  methylome_metadata meta;

  [[nodiscard]] static auto
  read(const std::string &dirname, const std::string &methylome_name,
       std::error_code &ec) -> methylome;

  [[nodiscard]] auto
  is_consistent() const -> bool;

  [[nodiscard]] auto
  write(const std::string &outdir,
        const std::string &name) const -> std::error_code;

  [[nodiscard]] auto
  init_metadata(const cpg_index &index) -> std::error_code;

  [[nodiscard]] auto
  update_metadata() -> std::error_code;
};

[[nodiscard]] inline auto
size(const methylome &m) -> std::size_t {
  return size(m.data);
}

[[nodiscard]] auto
methylome_files_exist(const std::string &directory,
                      const std::string &methylome_name) -> bool;

[[nodiscard]] auto
list_methylomes(const std::string &dirname,
                std::error_code &ec) -> std::vector<std::string>;

[[nodiscard]] auto
get_methylome_name_from_filename(const std::string &filename) -> std::string;

// methylome error codes

enum class methylome_code : std::uint32_t {
  ok = 0,
  invalid_methylome_data = 1,
};

template <>
struct std::is_error_code_enum<methylome_code> : public std::true_type {};

struct methylome_category : std::error_category {
  auto
  name() const noexcept -> const char * override {
    return "methylome";
  }
  auto
  message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    // clang-format off
    switch (code) {
    case 0: return "ok"s;
    case 1: return "invalid methylome data"s;
    }
    // clang-format on
    std::unreachable();
  }
};

inline auto
make_error_code(methylome_code e) -> std::error_code {
  static auto category = methylome_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_METHYLOME_HPP_
