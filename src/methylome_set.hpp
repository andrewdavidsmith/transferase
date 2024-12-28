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

#ifndef SRC_METHYLOME_SET_HPP_
#define SRC_METHYLOME_SET_HPP_

#include "methylome.hpp"

#include "ring_buffer.hpp"

#include <cstdint>  // std::uint32_t
#include <memory>   // std::shared_ptr, std::swap
#include <mutex>
#include <string>
#include <system_error>
#include <type_traits>  // for std::true_type
#include <unordered_map>
#include <utility>

struct methylome_set {
  methylome_set(const methylome_set &) = delete;
  methylome_set &
  operator=(const methylome_set &) = delete;

  explicit methylome_set(
    const std::string &methylome_directory,
    const std::uint32_t max_live_methylomes = default_max_live_methylomes) :
    methylome_directory{methylome_directory},
    max_live_methylomes{max_live_methylomes}, accessions{max_live_methylomes} {}

  [[nodiscard]] auto
  get_methylome(const std::string &accession,
                std::error_code &ec) -> std::shared_ptr<methylome>;

  static constexpr std::uint32_t default_max_live_methylomes{128};

  std::mutex mtx;
  std::string methylome_directory;
  std::uint32_t max_live_methylomes{};

  ring_buffer<std::string> accessions;
  std::unordered_map<std::string, std::shared_ptr<methylome>>
    accession_to_methylome;
};

// methylome_set errors
enum class methylome_set_code : std::uint32_t {
  ok = 0,
  error_loading_methylome = 1,
  methylome_not_found = 2,
  unknown_error = 3,
};
template <>
struct std::is_error_code_enum<methylome_set_code> : public std::true_type {};
struct methylome_set_category : std::error_category {
  // clang-format off
  auto name() const noexcept -> const char * override {return "methylome_set";}
  auto message(int code) const -> std::string override {
    using std::string_literals::operator""s;
    switch (code) {
    case 0: return "ok"s;
    case 1: return "error loading methylome"s;
    case 2: return "methylome not found"s;
    case 3: return "methylome set unknown error"s;
    }
    std::unreachable();
  }
  // clang-format on
};
inline auto
make_error_code(methylome_set_code e) -> std::error_code {
  static auto category = methylome_set_category{};
  return std::error_code(std::to_underlying(e), category);
}

#endif  // SRC_METHYLOME_SET_HPP_
