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

#include "methylome_set.hpp"

#include "methylome.hpp"

#include <iterator>  // for std::cend
#include <memory>    // for std::shared_ptr, std::make_shared
#include <mutex>     // for std::scoped_lock
#include <string>
#include <unordered_map>
#include <utility>  // for std::move, std::pair

namespace transferase {

[[nodiscard]] auto
methylome_set::get_methylome(const std::string &accession, std::error_code &ec)
  -> std::shared_ptr<methylome> {
  // ADS: make sure the error code starts out ok
  ec = std::error_code{};

  if (!methylome::is_valid_name(accession)) {
    ec = methylome_code::invalid_accession;
    return nullptr;
  }

  std::scoped_lock lock{mtx};

  // check if methylome is loaded
  const auto meth_itr = accession_to_methylome.find(accession);
  if (meth_itr != std::cend(accession_to_methylome)) {
    return meth_itr->second;
  }

  // ADS: we need to load a methylome; make sure the file exists;
  // probably should check the directory in batch
  if (!methylome::files_exist(methylome_directory, accession)) {
    ec = methylome_set_code::methylome_not_found;
    return nullptr;
  }

  auto loaded_meth = methylome::read(methylome_directory, accession, ec);
  if (ec) {
    // ADS: need to ensure the error code is sensibly propagated
    return nullptr;
  }

  // remove loaded methylomes if we need to make room
  if (accessions.full()) {
    const auto to_eject_itr = accession_to_methylome.find(accessions.front());
    if (to_eject_itr == std::cend(accession_to_methylome)) {
      ec = methylome_set_code::error_loading_methylome;
      return nullptr;
    }
    accession_to_methylome.erase(to_eject_itr);
    // ADS: no need to pop from the accessions, it will happen on push
    // since accessions was full.
  }

  const auto insertion_result = accession_to_methylome.emplace(
    accession, std::make_shared<methylome>(std::move(loaded_meth)));
  if (!insertion_result.second) {
    ec = methylome_set_code::unknown_error;
    return nullptr;
  }

  // ADS: if we are here, then everything went ok and we can insert
  // the accession into the ring buffer
  accessions.push_back(accession);

  return insertion_result.first->second;
}

}  // namespace transferase
