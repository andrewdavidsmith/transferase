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

#include "xfrase_error.hpp"  // for make_error_code, methylome_set_code

#include <memory>  // for std::shared_ptr, std::make_shared
#include <mutex>   // for std::scoped_lock
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>  // for std::move, std::pair

[[nodiscard]] auto
methylome_set::get_methylome(const std::string &accession, std::error_code &ec)
  -> std::shared_ptr<methylome> {
  if (!is_valid_accession(accession)) {
    ec = methylome_set_code::invalid_accession;
    return nullptr;
  }

  std::unordered_map<std::string, std::shared_ptr<methylome>>::const_iterator
    meth{};

  std::scoped_lock lock{mtx};

  // check if methylome is loaded
  const auto meth_itr = accession_to_methylome.find(accession);
  if (meth_itr == std::cend(accession_to_methylome)) {
    // ADS: might not be needed as separate function, but ok for now
    if (!methylome_files_exist(methylome_directory, accession)) {
      ec = methylome_set_code::methylome_file_not_found;
      return nullptr;
    }

    // take care of removing loaded methylomes if needed to make room
    const std::string to_eject = accessions.push(accession);
    if (!to_eject.empty()) {
      const auto to_eject_itr = accession_to_methylome.find(to_eject);
      if (to_eject_itr == std::cend(accession_to_methylome)) {
        ec = methylome_set_code::error_updating_live_methylomes;
        return nullptr;
      }
      accession_to_methylome.erase(to_eject_itr);
    }

    const auto loaded_meth = read_methylome(methylome_directory, accession, ec);
    if (ec)
      return nullptr;

    bool insertion_ok{false};
    std::tie(meth, insertion_ok) = accession_to_methylome.emplace(
      accession, std::make_shared<methylome>(std::move(loaded_meth)));
    if (!insertion_ok) {
      ec = methylome_set_code::unknown_error;
      return nullptr;
    }
  }
  else if (meth_itr == std::cend(accession_to_methylome)) {
    ec = methylome_set_code::methylome_already_live;
    return nullptr;
  }
  else {
    meth = meth_itr;  // already loaded
  }

  ec = methylome_set_code::ok;
  return meth->second;
}
