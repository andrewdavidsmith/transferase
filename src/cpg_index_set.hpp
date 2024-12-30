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

#ifndef SRC_CPG_INDEX_SET_HPP_
#define SRC_CPG_INDEX_SET_HPP_

#include "cpg_index.hpp"

#include <cstdint>  // for std::uint32_t
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <unordered_map>
#include <utility>  // for std::to_underlying, std::unreachable

namespace xfrase {

struct cpg_index_set {
  cpg_index_set(const cpg_index_set &) = delete;
  cpg_index_set &
  operator=(const cpg_index_set &) = delete;

  // ADS: this cpg_index_set constructor always attempts to read files
  // so the error code is needed here; this contrasts with
  // methylome_set, which does no such work until requested.
  cpg_index_set(const std::string &cpg_index_directory, std::error_code &ec);

  [[nodiscard]] auto
  get_cpg_index(const std::string &assembly,
                std::error_code &ec) -> std::shared_ptr<cpg_index>;

  std::unordered_map<std::string, std::shared_ptr<cpg_index>>
    assembly_to_cpg_index;
};

}  // namespace xfrase

// error code for cpg_index_set
enum class cpg_index_set_error : std::uint32_t {
  ok = 0,
  cpg_index_not_found = 1,
};

template <>
struct std::is_error_code_enum<cpg_index_set_error> : public std::true_type {};

struct cpg_index_set_category : std::error_category {
  auto
  name() const noexcept -> const char * override {
    return "cpg_index_set";
  }
  auto
  message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    // clang-format off
    switch (code) {
    case 0: return "ok"s;
    case 1: return "cpg index not found"s;
    }
    // clang-format on
    std::unreachable();  // hopefully
  }
};

inline auto
make_error_code(cpg_index_set_error e) -> std::error_code {
  static auto category = cpg_index_set_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_CPG_INDEX_SET_HPP_
