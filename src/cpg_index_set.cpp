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
#include "mxe_error.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <regex>
#include <string>
#include <tuple>
#include <unordered_map>

[[nodiscard]] auto
cpg_index_set::get_genome_sizes()
  -> std::unordered_map<std::string, std::uint64_t> {
  std::unordered_map<std::string, std::uint64_t> r;
  for (const auto [assembly, idx] : assembly_to_cpg_index)
    r.emplace(assembly, std::reduce(std::cbegin(idx.chrom_size),
                                    std::cend(idx.chrom_size)));
  return r;
}

[[nodiscard]] auto
cpg_index_set::get_cpg_index(const std::string &assembly_name)
  -> std::tuple<const cpg_index &, std::error_code> {
  const auto itr = assembly_to_cpg_index.find(assembly_name);
  if (itr == std::cend(assembly_to_cpg_index))
    return {{}, std::make_error_code(std::errc::invalid_argument)};
  return {itr->second, {}};
}

cpg_index_set::cpg_index_set(const std::string &cpg_index_directory) :
  cpg_index_directory{cpg_index_directory} {
  static constexpr auto assembly_ptrn = R"(^[_[:alnum:]]+)";
  static const auto cpg_index_filename_ptrn =
    std::format(R"({}.{}$)", assembly_ptrn, cpg_index::filename_extension);
  std::regex cpg_index_filename_re(cpg_index_filename_ptrn);
  std::regex assembly_re(assembly_ptrn);

  const std::filesystem::path idx_dir{cpg_index_directory};
  for (auto const &dir_entry : std::filesystem::directory_iterator{idx_dir}) {
    const std::string name = dir_entry.path().filename().string();
    if (std::regex_search(name, cpg_index_filename_re)) {
      std::smatch base_match;
      if (std::regex_search(name, base_match, assembly_re)) {
        const auto assembly = base_match[0].str();
        cpg_index index{};
        const std::string filename = dir_entry.path().string();
        if (const auto ec = index.read(filename); ec) {
          logger::instance().debug("Failed to read cpg index: {} ({})",
                                   filename, ec);
          assembly_to_cpg_index.clear();
          return;
        }
        assembly_to_cpg_index.emplace(assembly, index);
      }
    }
  }
}
