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

#include "cpg_index_set.hpp"

#include "cpg_index.hpp"
#include "logger.hpp"
#include "xfrase_error.hpp"  // IWYU pragma: keep

#include <iterator>  // for std::cend
#include <memory>    // for std::make_shared
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>  // for std::move, std::pair
#include <vector>

namespace xfrase {

[[nodiscard]] auto
cpg_index_set::get_cpg_index(const std::string &assembly, std::error_code &ec)
  -> std::shared_ptr<cpg_index> {
  const auto itr_index = assembly_to_cpg_index.find(assembly);
  if (itr_index == std::cend(assembly_to_cpg_index)) {
    ec = cpg_index_set_error::cpg_index_not_found;
    return nullptr;
  }
  ec = cpg_index_set_error::ok;
  return itr_index->second;
}

cpg_index_set::cpg_index_set(const std::string &cpg_index_directory,
                             std::error_code &ec) {
  const auto genome_names = list_cpg_indexes(cpg_index_directory, ec);
  if (ec)
    return;
  for (const auto &name : genome_names) {
    const auto index = cpg_index::read(cpg_index_directory, name, ec);
    if (ec) {
      logger::instance().error("Failed to read cpg index {} {}: {}",
                               cpg_index_directory, name, ec);
      assembly_to_cpg_index.clear();
      return;
    }
    assembly_to_cpg_index.emplace(name, std::make_shared<cpg_index>(index));
  }
}

}  // namespace xfrase
