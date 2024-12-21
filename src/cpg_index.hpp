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

#if not defined(__APPLE__) && not defined(__MACH__)
#include "aligned_allocator.hpp"  // for aligned_allocator
#endif

#include <cstdint>  // for std::uint32_t, std::int32_t, std::uint64_t
#include <string>
#include <system_error>
#include <tuple>
#include <utility>  // for std::pair
#include <vector>

struct genomic_interval;
struct cpg_index_metadata;

struct cpg_index {
  // includes the dot because that's how std::filesystem::path works
  static constexpr auto filename_extension{".cpg_idx"};

  typedef std::uint32_t cpg_pos_t;
#if not defined(__APPLE__) && not defined(__MACH__)
  typedef std::vector<cpg_pos_t, aligned_allocator<cpg_pos_t>> vec;
#else
  typedef std::vector<cpg_pos_t> vec;
#endif

  [[nodiscard]] static auto
  read(const cpg_index_metadata &cim,
       const std::string &index_file) -> std::tuple<cpg_index, std::error_code>;
  [[nodiscard]] auto
  write(const std::string &index_file) const -> std::error_code;

  [[nodiscard]] auto
  hash() const -> std::uint64_t;

  [[nodiscard]] auto
  get_n_cpgs() const -> std::uint32_t;

  [[nodiscard]] auto
  get_offsets_within_chrom(
    const std::int32_t ch_id,
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> &pos) const
    -> std::vector<std::pair<std::uint32_t, std::uint32_t>>;

  [[nodiscard]] auto
  get_offsets(const std::int32_t ch_id, const cpg_index_metadata &cim,
              const std::vector<std::pair<std::uint32_t, std::uint32_t>> &pos)
    const -> std::vector<std::pair<std::uint32_t, std::uint32_t>>;

  [[nodiscard]] auto
  get_offsets(const cpg_index_metadata &cim,
              const std::vector<genomic_interval> &gis) const
    -> std::vector<std::pair<std::uint32_t, std::uint32_t>>;

  std::vector<cpg_index::vec> positions;
};

[[nodiscard]] auto
initialize_cpg_index(const std::string &genome_file)
  -> std::tuple<cpg_index, cpg_index_metadata, std::error_code>;

[[nodiscard]] auto
read_cpg_index(const std::string &index_file)
  -> std::tuple<cpg_index, cpg_index_metadata, std::error_code>;

[[nodiscard]] auto
read_cpg_index(const std::string &index_file,
               const std::string &index_meta_file)
  -> std::tuple<cpg_index, cpg_index_metadata, std::error_code>;

#endif  // SRC_CPG_INDEX_HPP_
