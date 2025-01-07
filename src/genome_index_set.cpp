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

#include "genome_index_set.hpp"

#include "genome_index.hpp"
#include "logger.hpp"

#include <iterator>  // for std::cend
#include <memory>    // for std::make_shared
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>  // for std::move, std::pair
#include <vector>

namespace transferase {

[[nodiscard]] auto
genome_index_set::get_genome_index(const std::string &assembly,
                                   std::error_code &ec)
  -> std::shared_ptr<genome_index> {
  const auto itr_index = assembly_to_genome_index.find(assembly);
  if (itr_index == std::cend(assembly_to_genome_index)) {
    ec = genome_index_set_error::genome_index_not_found;
    return nullptr;
  }
  ec = genome_index_set_error::ok;
  return itr_index->second;
}

genome_index_set::genome_index_set(const std::string &directory,
                                   std::error_code &ec) {
  const auto genome_names = genome_index::list_genome_indexes(directory, ec);
  if (ec)
    return;
  for (const auto &name : genome_names) {
    const auto index = genome_index::read(directory, name, ec);
    if (ec) {
      logger::instance().error("Failed to read cpg index {} {}: {}", directory,
                               name, ec.message());
      assembly_to_genome_index.clear();
      return;
    }
    assembly_to_genome_index.emplace(name,
                                     std::make_shared<genome_index>(index));
  }
}

}  // namespace transferase
