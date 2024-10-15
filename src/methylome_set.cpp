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

#include "mc16_error.hpp"
#include "methylome.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>  // std::make_shared
#include <mutex>
#include <print>
#include <ranges>
#include <regex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>  // std::move
#include <vector>

using std::make_shared;
using std::mutex;
using std::println;
using std::size_t;
using std::string;
using std::tuple;
using std::unordered_map;
using std::vector;

namespace rg = std::ranges;
namespace fs = std::filesystem;

[[nodiscard]] static auto
is_valid_accession(const string &accession) -> bool {
  static constexpr auto experiment_ptrn = R"(^(D|E|S)RX\d+$)";
  std::regex experiment_re(experiment_ptrn);
  return std::regex_search(accession, experiment_re);
}

[[nodiscard]] auto
methylome_set::get_methylome(const string &accession)
  -> tuple<std::shared_ptr<methylome>, std::error_code> {
  if (!is_valid_accession(accession))
    return {nullptr, methylome_set_code::invalid_accession};

  unordered_map<string, std::shared_ptr<methylome>>::const_iterator meth{};

  std::scoped_lock lock{mtx};

  // check if methylome is loaded
  const auto acc_meth_itr = accession_to_methylome.find(accession);
  if (acc_meth_itr == cend(accession_to_methylome)) {
    const auto filename = format("{}/{}.mc16", mc16_directory, accession);
    if (!fs::exists(filename))
      return {nullptr, methylome_set_code::methylome_file_not_found};

    const string to_eject = accessions.push(accession);
    if (!to_eject.empty()) {
      const auto to_eject_itr = accession_to_methylome.find(to_eject);
      if (to_eject_itr == cend(accession_to_methylome))
        return {nullptr, methylome_set_code::error_updating_live_methylomes};

      accession_to_methylome.erase(to_eject_itr);
    }

    // ADS: get an error code from methylome::read and use it
    methylome m{};
    if ([[maybe_unused]] const auto ec = m.read(filename))
      return {nullptr, methylome_set_code::error_reading_methylome_file};

    bool insertion_happened{false};
    std::tie(meth, insertion_happened) = accession_to_methylome.emplace(
      accession, make_shared<methylome>(std::move(m)));
    if (!insertion_happened)
      return {nullptr, methylome_set_code::methylome_already_live};
  }
  else
    meth = acc_meth_itr;  // already loaded

  // ADS TODO: use this as error condition
  n_total_cpgs = size(meth->second->cpgs);

  return {meth->second, methylome_set_code::ok};
}

[[nodiscard]] auto
methylome_set::summary() const -> string {
  constexpr auto fmt = "n_live_methylomes: {}\n"
                       "max_live_methylomes: {}\n"
                       "mc16_directory: {}\n"
                       "methylomes:";
  string r{format(fmt, accessions.size(), max_live_methylomes, mc16_directory)};
  rg::for_each(accessions, [&](const auto &x) { r += format("\n{}", x); });
  return r;
}
