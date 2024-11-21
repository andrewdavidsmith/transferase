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
#include "mxe_error.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <memory>  // std::make_shared
#include <mutex>
#include <regex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>  // std::move

[[nodiscard]] static auto
is_valid_accession(const std::string &accession) -> bool {
  static constexpr auto experiment_ptrn = R"(^(D|E|S)RX\d+$)";
  std::regex experiment_re(experiment_ptrn);
  return std::regex_search(accession, experiment_re);
}

[[nodiscard]] auto
methylome_set::get_methylome(const std::string &accession)
  -> std::tuple<std::shared_ptr<methylome>, std::error_code> {
  static constexpr auto methylome_filename_format = "{}/{}.{}";

  if (!is_valid_accession(accession))
    return {nullptr, methylome_set_code::invalid_accession};

  std::unordered_map<std::string, std::shared_ptr<methylome>>::const_iterator
    meth{};

  std::scoped_lock lock{mtx};

  // check if methylome is loaded
  const auto acc_meth_itr = accession_to_methylome.find(accession);
  if (acc_meth_itr == std::cend(accession_to_methylome)) {
    const auto filename =
      std::format(methylome_filename_format, methylome_directory, accession,
                  methylome_extension);
    if (!std::filesystem::exists(filename))
      return {nullptr, methylome_set_code::methylome_file_not_found};

    const std::string to_eject = accessions.push(accession);
    if (!to_eject.empty()) {
      const auto to_eject_itr = accession_to_methylome.find(to_eject);
      if (to_eject_itr == std::cend(accession_to_methylome))
        return {nullptr, methylome_set_code::error_updating_live_methylomes};

      accession_to_methylome.erase(to_eject_itr);
    }

    // ADS: get an error code from methylome::read and use it
    methylome m{};
    if ([[maybe_unused]] const auto ec = m.read(filename))
      return {nullptr, methylome_set_code::error_reading_methylome_file};

    bool insertion_happened{false};
    std::tie(meth, insertion_happened) = accession_to_methylome.emplace(
      accession, std::make_shared<methylome>(std::move(m)));
    if (!insertion_happened)
      return {nullptr, methylome_set_code::methylome_already_live};
  }
  else
    meth = acc_meth_itr;  // already loaded

  // ADS TODO: use this as error condition
  n_total_cpgs = std::size(meth->second->cpgs);

  return {meth->second, methylome_set_code::ok};
}
