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

#include <cstdint>
#include <string>
#include <vector>
#include <tuple>
#include <regex>
#include <iostream>
#include <unordered_map>
#include <filesystem>
#include <print>
#include <ranges>
#include <algorithm>
#include <mutex>

using std::string;
using std::vector;
using std::size_t;
using std::tuple;
using std::unordered_map;
using std::println;
using std::make_shared;
using std::mutex;

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
  -> tuple<std::shared_ptr<methylome>, int> {
  if (!is_valid_accession(accession)) {
    if (verbose)
      println(std::cout, "not valid accession: {}", accession);
    return {nullptr, -1};
  }
  if (verbose)
    println(std::cout, "valid accession: {}", accession);

  unordered_map<string, std::shared_ptr<methylome>>::const_iterator meth{};

  std::scoped_lock lock{mtx};

  // ADS TODO: add error codes for return values

  // check if methylome is loaded
  const auto acc_meth_itr = accession_to_methylome.find(accession);
  if (acc_meth_itr == cend(accession_to_methylome)) {
    if (verbose)
      println(std::cout, "Methylome not live: {}", accession);
    const auto filename = format("{}/{}.mc16", mc16_directory, accession);
    if (verbose)
      println(std::cout, "Methylome filename: {}", filename);
    if (!fs::exists(filename)) {
      if (verbose)
        println(std::cout, "File not found: {}", filename);
      return {nullptr, -1};
    }
    const string to_eject = accessions.push(accession);
    if (!to_eject.empty()) {
      const auto to_eject_itr = accession_to_methylome.find(to_eject);
      if (to_eject_itr == cend(accession_to_methylome)) {
        if (verbose)
          println(std::cout, "Error ejecting methylome: {}", to_eject);
        return {nullptr, -2};
      }
      accession_to_methylome.erase(to_eject_itr);
    }
    methylome m{};
    if (const auto ec = m.read(filename)) {
      if (verbose)
        println(std::cout, "Error reading methylome file: {} (error={})",
                filename, ec);
      return {nullptr, -1};
    }

    std::tie(meth, std::ignore) =
      accession_to_methylome.emplace(accession,
                                     make_shared<methylome>(std::move(m)));
  }
  else
    meth = acc_meth_itr;  // already loaded

  n_total_cpgs = size(meth->second->cpgs);

  return {meth->second, 0};
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
