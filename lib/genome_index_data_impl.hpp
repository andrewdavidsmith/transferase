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

#ifndef LIB_GENOME_INDEX_DATA_IMPL_HPP_
#define LIB_GENOME_INDEX_DATA_IMPL_HPP_

#ifdef UNIT_TEST
#define STATIC
#else
#define STATIC static
#endif

#include "genome_index_data.hpp"
#include <vector>

namespace transferase {

struct chrom_range_t;
struct query_container;

[[nodiscard]] STATIC auto
make_query_within_chrom(const genome_index_data::vec &positions,
                        const std::vector<chrom_range_t> &chrom_ranges) noexcept
  -> transferase::query_container;

}  // namespace transferase

#endif  // LIB_GENOME_INDEX_DATA_IMPL_HPP_
