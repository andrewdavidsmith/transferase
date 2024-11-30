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
#include "cpg_index_meta.hpp"

#include <cstdint>  // for std::uint64_t
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

struct cpg_index_set {
  cpg_index_set(const cpg_index_set &) = delete;
  cpg_index_set &
  operator=(const cpg_index_set &) = delete;

  explicit cpg_index_set(const std::string &cpg_index_directory,
                         std::error_code &ec);

  // [[nodiscard]] static auto
  // read(const std::string &filename)
  //   -> std::tuple<cpg_index_set, std::error_code>;

  /* ADS: currently unused */
  // [[nodiscard]] auto
  // get_cpg_index(const std::string &assembly_name)
  // -> std::tuple<const cpg_index &, std::error_code>;

  [[nodiscard]] auto
  get_cpg_index_meta(const std::string &assembly_name)
    -> std::tuple<const cpg_index_meta &, std::error_code>;

  [[nodiscard]] auto
  get_cpg_index_with_meta(const std::string &assembly_name)
    -> std::tuple<const cpg_index &, const cpg_index_meta &, std::error_code>;

  /* ADS: currently unused */
  // [[nodiscard]] auto
  // get_genome_sizes() -> std::unordered_map<std::string, std::uint64_t>;

  std::unordered_map<std::string, cpg_index> assembly_to_cpg_index;
  std::unordered_map<std::string, cpg_index_meta> assembly_to_cpg_index_meta;
};

#endif  // SRC_CPG_INDEX_SET_HPP_
