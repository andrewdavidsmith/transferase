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

#ifndef SRC_CPG_INDEX_IMPL_HPP_
#define SRC_CPG_INDEX_IMPL_HPP_

#ifdef UNIT_TEST
#define STATIC
#else
#define STATIC static
#endif

#include "cpg_index_data.hpp"

#include <cstddef>  // std::size_t
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace transferase {

struct genome_file {
  std::error_code ec{};
  char *data{};
  std::size_t sz{};
};

[[nodiscard]] STATIC auto
mmap_genome(const std::string &filename) -> genome_file;

[[nodiscard]] STATIC auto
cleanup_mmap_genome(genome_file &gf) -> std::error_code;

[[nodiscard]] STATIC auto
get_cpgs(const std::string_view chrom) -> cpg_index_data::vec;

[[nodiscard]] STATIC auto
get_chrom_name_starts(const char *data,
                      const std::size_t sz) -> std::vector<std::size_t>;

[[nodiscard]] STATIC auto
get_chrom_name_stops(std::vector<std::size_t> starts, const char *data,
                     const std::size_t sz) -> std::vector<std::size_t>;

[[nodiscard]] STATIC auto
get_chroms(const char *data, const std::size_t sz,
           const std::vector<std::size_t> &name_starts,
           const std::vector<std::size_t> &name_stops)
  -> std::vector<std::string_view>;

}  // namespace transferase

#endif  // SRC_CPG_INDEX_IMPL_HPP_
