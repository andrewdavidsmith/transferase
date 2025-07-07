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
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>  // for std::move, std::pair

#ifdef BENCHMARK
#include "logger.hpp"
#include <chrono>
#endif

namespace transferase {

[[nodiscard]] auto
methylome_set::get_methylome(const std::string &methylome_name,
                             std::error_code &ec)
  -> std::shared_ptr<methylome> {
  // make sure error code is clear
  ec = std::error_code{};

  if (!methylome::is_valid_name(methylome_name)) {
    ec = methylome_error_code::invalid_methylome_name;
    return nullptr;
  }

  const auto cached = [&] {
    std::shared_lock read_lock{mtx};
    const auto meth_itr = accession_to_methylome.find(methylome_name);
    if (meth_itr != std::cend(accession_to_methylome))
      return meth_itr->second;
    return std::shared_ptr<methylome>{};  // return nullptr
  }();  // shared lock released here

  if (cached) {
    std::unique_lock write_lock{mtx};
    accessions.move_to_front(methylome_name);
    return cached;
  }  // unique lock released here

  // Not found in cache; load methylome (no lock needed during IO).
  // ADS: we need to load a methylome; make sure the file exists;
  // probably should check the directory in batch
  if (!methylome::files_exist(methylome_dir, methylome_name)) {
    ec = methylome_error_code::methylome_not_found;
    return nullptr;
  }

#ifdef BENCHMARK
  auto &lgr = logger::instance();
  const auto before_read = std::chrono::high_resolution_clock::now();
#endif
  auto loaded_meth = methylome::read(methylome_dir, methylome_name, ec);
#ifdef BENCHMARK
  const auto after_read = std::chrono::high_resolution_clock::now();
  auto delta = after_read - before_read;
  lgr.debug(
    "methylome read time: {}us",
    std::chrono::duration_cast<std::chrono::microseconds>(delta).count());
#endif
  if (ec) {
    return nullptr;
  }

  // Now update cache with unique lock
#ifdef BENCHMARK
  const auto before_write_lock = std::chrono::high_resolution_clock::now();
#endif
  std::unique_lock write_lock{mtx};
#ifdef BENCHMARK
  const auto after_write_lock = std::chrono::high_resolution_clock::now();
  delta = after_write_lock - before_write_lock;
  lgr.debug(
    "write lock wait time: {}us",
    std::chrono::duration_cast<std::chrono::microseconds>(delta).count());
#endif

  // Double-check if another thread inserted this methylome while we were
  // loading
  const auto meth_itr_after_load = accession_to_methylome.find(methylome_name);
  if (meth_itr_after_load != std::cend(accession_to_methylome)) {
    // Another thread won the race; update LRU and return existing
    accessions.move_to_front(methylome_name);
    return meth_itr_after_load->second;
  }

  // Evict if full
  if (accessions.full()) {
    const auto to_eject_itr = accession_to_methylome.find(accessions.back());
    if (to_eject_itr == std::cend(accession_to_methylome)) {
      ec = methylome_error_code::error_reading_methylome;
      return nullptr;
    }
    accession_to_methylome.erase(to_eject_itr);
    // no need to pop from the accessions, it will happen on push
    // since accessions was full.
  }

  // Insert newly loaded methylome
  const auto insertion_result = accession_to_methylome.emplace(
    methylome_name, std::make_shared<methylome>(std::move(loaded_meth)));
  if (!insertion_result.second) {
    ec = methylome_error_code::unknown_error;
    return nullptr;
  }

  accessions.push(methylome_name);

  return insertion_result.first->second;
}

}  // namespace transferase
