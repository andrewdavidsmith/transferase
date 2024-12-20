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
#include "methylome_metadata.hpp"
#include "xfrase_error.hpp"  // for make_error_code, methylome_set_code

#include <filesystem>
#include <format>
#include <memory>  // for std::shared_ptr, std::make_shared
#include <mutex>   // for std::scoped_lock
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>  // for std::move, std::pair

[[nodiscard]] auto
methylome_set::get_methylome(const std::string &accession)
  -> std::tuple<std::shared_ptr<methylome>, std::shared_ptr<methylome_metadata>,
                std::error_code> {
  static constexpr auto filename_format = "{}/{}{}";

  if (!is_valid_accession(accession))
    return {nullptr, nullptr, methylome_set_code::invalid_accession};

  // clang-format off
  std::unordered_map<std::string, std::shared_ptr<methylome>>::const_iterator meth{};
  std::unordered_map<std::string, std::shared_ptr<methylome_metadata>>::const_iterator meta{};
  // clang-format on

  std::scoped_lock lock{mtx};

  // check if methylome is loaded
  const auto meth_itr = accession_to_methylome.find(accession);
  const auto meta_itr = accession_to_methylome_metadata.find(accession);
  if (meth_itr == std::cend(accession_to_methylome) &&
      meta_itr == std::cend(accession_to_methylome_metadata)) {
    const auto methylome_filename =
      std::format(filename_format, methylome_directory, accession,
                  methylome::filename_extension);
    if (!std::filesystem::exists(methylome_filename))
      return {nullptr, nullptr, methylome_set_code::methylome_file_not_found};

    const auto metadata_filename =
      std::format(filename_format, methylome_directory, accession,
                  methylome_metadata::filename_extension);
    if (!std::filesystem::exists(metadata_filename))
      return {nullptr, nullptr,
              methylome_set_code::methylome_metadata_file_not_found};

    const std::string to_eject = accessions.push(accession);
    if (!to_eject.empty()) {
      const auto to_eject_meth_itr = accession_to_methylome.find(to_eject);
      const auto to_eject_meta_itr =
        accession_to_methylome_metadata.find(to_eject);
      if (to_eject_meth_itr == std::cend(accession_to_methylome) ||
          to_eject_meta_itr == std::cend(accession_to_methylome_metadata))
        return {nullptr, nullptr,
                methylome_set_code::error_updating_live_methylomes};

      accession_to_methylome.erase(to_eject_meth_itr);
      accession_to_methylome_metadata.erase(to_eject_meta_itr);
    }

    const auto [mm, meta_ec] = methylome_metadata::read(metadata_filename);
    if (meta_ec)
      return {nullptr, nullptr,
              methylome_set_code::error_reading_methylome_file};

    // ADS: get an error code from methylome::read and use it
    const auto [m, ec] = methylome::read(methylome_filename, mm);
    if (ec)
      return {nullptr, nullptr,
              methylome_set_code::error_reading_methylome_file};

    bool insertion_happened{false};
    std::tie(meth, insertion_happened) = accession_to_methylome.emplace(
      accession, std::make_shared<methylome>(std::move(m)));
    if (!insertion_happened)
      return {nullptr, nullptr, methylome_set_code::methylome_already_live};

    std::tie(meta, insertion_happened) =
      accession_to_methylome_metadata.emplace(
        accession, std::make_shared<methylome_metadata>(std::move(mm)));
    if (!insertion_happened)
      return {nullptr, nullptr, methylome_set_code::methylome_already_live};
  }
  else if (meth_itr == std::cend(accession_to_methylome) ||
           meta_itr == std::cend(accession_to_methylome_metadata)) {
    return {nullptr, nullptr, methylome_set_code::methylome_already_live};
  }
  else {
    meth = meth_itr;  // already loaded
    meta = meta_itr;
  }

  return {meth->second, meta->second, methylome_set_code::ok};
}
