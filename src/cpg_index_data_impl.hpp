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

#ifndef SRC_CPG_INDEX_DATA_IMPL_HPP_
#define SRC_CPG_INDEX_DATA_IMPL_HPP_

#ifdef UNIT_TEST
#define STATIC
#else
#define STATIC static
#endif

#include <cstddef>  // std::size_t
#include <string>
#include <system_error>
#include <tuple>
#include <vector>

[[nodiscard]] STATIC auto
get_offsets_within_chrom(
  const cpg_index_data::vec &positions,
  const std::vector<std::pair<std::uint32_t, std::uint32_t>> &queries)
  -> std::vector<std::pair<std::uint32_t, std::uint32_t>>;

[[nodiscard]] STATIC auto
add_offsets_within_chrom(const cpg_index_data::vec &positions, auto queries_beg,
                         const auto queries_end);

#endif  // SRC_CPG_INDEX_DATA_IMPL_HPP_
