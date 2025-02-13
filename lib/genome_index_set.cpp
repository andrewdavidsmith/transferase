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
#include "ring_buffer.hpp"

#include <iterator>  // for std::cend
#include <memory>    // for std::shared_ptr, std::make_shared
#include <mutex>     // for std::scoped_lock
#include <string>
#include <unordered_map>
#include <utility>  // for std::move, std::pair

namespace transferase {

[[nodiscard]] auto
genome_index_set::get_genome_index(const std::string &genome_name,
                                   std::error_code &ec)
  -> std::shared_ptr<genome_index> {
  // ADS: error code passed by reference; make sure it starts out ok
  ec = std::error_code{};

  if (!genome_index::is_valid_name(genome_name)) {
    ec = genome_index_error_code::invalid_genome_name;
    return nullptr;
  }

  std::scoped_lock lock{mtx};

  // Easy case: genome index is already loaded
  const auto index_itr = name_to_index.find(genome_name);
  if (index_itr != std::cend(name_to_index))
    return index_itr->second;

  // ADS: we need to load a genome index; make sure the file exists
  if (!genome_index::files_exist(genome_index_directory, genome_name)) {
    ec = genome_index_set_error_code::genome_index_not_found;
    return nullptr;
  }

  auto loaded_genome_index =
    genome_index::read(genome_index_directory, genome_name, ec);
  if (ec) {
    // ADS: the error code passed back will have come from genome_index::read
    return nullptr;
  }

  // ADS: remove a loaded genome index from the FIFO if we need to
  // make room
  if (genome_names.full()) {
    const auto to_eject_itr = name_to_index.find(genome_names.front());
    if (to_eject_itr == std::cend(name_to_index)) {
      ec = genome_index_set_error_code::error_loading_genome_index;
      return nullptr;
    }
    name_to_index.erase(to_eject_itr);
    // ADS: no need to pop from the genome_names FIFO as it will
    // happen below on push since genome_names was full.
  }

  const auto insertion_result = name_to_index.emplace(
    genome_name,
    std::make_shared<genome_index>(std::move(loaded_genome_index)));
  if (!insertion_result.second) {
    ec = genome_index_set_error_code::unknown_error;
    return nullptr;
  }

  // ADS: if we are here, then everything went ok and we can insert
  // the genome name into the FIFO
  genome_names.push_back(genome_name);

  return insertion_result.first->second;
}

}  // namespace transferase
