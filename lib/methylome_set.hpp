/* MIT License
 *
 * Copyright (c) 2024-2026 Andrew D Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LIB_METHYLOME_SET_HPP_
#define LIB_METHYLOME_SET_HPP_

#include "lru_tracker.hpp"

#include <cstdint>  // std::uint32_t
#include <memory>   // std::shared_ptr, std::swap
#include <shared_mutex>
#include <string>
#include <system_error>
#include <unordered_map>

namespace transferase {

struct methylome;

struct methylome_set {
  explicit methylome_set(
    const std::string &methylome_dir,
    const std::uint32_t max_live_methylomes = default_max_live_methylomes) :
    methylome_dir{methylome_dir}, max_live_methylomes{max_live_methylomes},
    accessions{max_live_methylomes} {}

  // prevent copy; move disallowed because of std::mutex member
  // clang-format off
  methylome_set(const methylome_set &) = delete;
  auto operator=(const methylome_set &) -> methylome_set & = delete;
  methylome_set(methylome_set &&) noexcept = delete;
  auto operator=(methylome_set &&) noexcept -> methylome_set & = delete;
  // clang-format on

  [[nodiscard]] auto
  get_methylome(const std::string &accession,
                std::error_code &ec) -> std::shared_ptr<methylome>;

  static constexpr std::uint32_t default_max_live_methylomes{128};

  std::shared_mutex mtx;
  std::string methylome_dir;
  std::uint32_t max_live_methylomes{};

  lru_tracker<std::string> accessions;
  std::unordered_map<std::string, std::shared_ptr<methylome>>
    accession_to_methylome;
};

}  // namespace transferase

#endif  // LIB_METHYLOME_SET_HPP_
