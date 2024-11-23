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

#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

struct cpg_index_set {
  cpg_index_set(const cpg_index_set &) = delete;
  cpg_index_set &operator=(const cpg_index_set &) = delete;

  explicit cpg_index_set(const std::string &cpg_index_directory);

  [[nodiscard]] auto get_cpg_index(const std::string &assembly_name)
    -> std::tuple<const cpg_index &, std::error_code>;

  std::string cpg_index_directory;
  std::unordered_map<std::string, cpg_index> assembly_to_cpg_index;
};

#endif  // SRC_CPG_INDEX_SET_HPP_
