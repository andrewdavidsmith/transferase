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
#include "cpg_index_metadata.hpp"
#include "logger.hpp"
#include "xfrase_error.hpp"  // IWYU pragma: keep

#include <filesystem>
#include <format>
#include <iterator>  // for std::cend
#include <regex>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <utility>  // for std::move, std::pair
#include <vector>

[[nodiscard]] auto
cpg_index_set::get_cpg_index_metadata(const std::string &assembly_name)
  -> std::tuple<const cpg_index_metadata &, std::error_code> {
  const auto itr = assembly_to_cpg_index_metadata.find(assembly_name);
  if (itr == std::cend(assembly_to_cpg_index_metadata))
    return {{}, std::make_error_code(std::errc::invalid_argument)};
  return {itr->second, {}};
}

[[nodiscard]] auto
cpg_index_set::get_cpg_index_with_meta(const std::string &assembly_name)
  -> std::tuple<const cpg_index &, const cpg_index_metadata &,
                std::error_code> {
  const auto itr_index = assembly_to_cpg_index.find(assembly_name);
  if (itr_index == std::cend(assembly_to_cpg_index))
    return {{}, {}, std::make_error_code(std::errc::invalid_argument)};
  const auto itr_meta = assembly_to_cpg_index_metadata.find(assembly_name);
  if (itr_meta == std::cend(assembly_to_cpg_index_metadata))
    return {{}, {}, std::make_error_code(std::errc::invalid_argument)};
  return {itr_index->second, itr_meta->second, {}};
}

cpg_index_set::cpg_index_set(const std::string &cpg_index_directory,
                             std::error_code &ec) {
  static constexpr auto assembly_ptrn = R"(^[_[:alnum:]]+)";
  static const auto cpg_index_filename_ptrn =
    std::format(R"({}{}$)", assembly_ptrn, cpg_index::filename_extension);
  std::regex cpg_index_filename_re(cpg_index_filename_ptrn);
  std::regex assembly_re(assembly_ptrn);

  std::unordered_map<std::string, cpg_index> assembly_to_cpg_index_in;
  std::unordered_map<std::string, cpg_index_metadata>
    assembly_to_cpg_index_metadata_in;

  const std::filesystem::path idx_dir{cpg_index_directory};
  for (auto const &dir_entry : std::filesystem::directory_iterator{idx_dir}) {
    const std::string name = dir_entry.path().filename().string();
    if (std::regex_search(name, cpg_index_filename_re)) {
      std::smatch base_match;
      if (std::regex_search(name, base_match, assembly_re)) {
        const auto assembly = base_match[0].str();
        const std::string index_filename = dir_entry.path().string();

        // read the cpg index metadata
        const auto meta_file =
          get_default_cpg_index_metadata_filename(index_filename);
        const auto [cim, meta_ec] = cpg_index_metadata::read(meta_file);
        if (meta_ec) {
          logger::instance().error("Failed to read cpg index metadata {}: {}",
                                   meta_file, meta_ec);
          ec = meta_ec;
          return;
        }
        assembly_to_cpg_index_metadata_in.emplace(assembly, cim);

        // read the cpg index
        const auto [index, index_ec] = cpg_index::read(cim, index_filename);
        if (index_ec) {
          logger::instance().error("Failed to read cpg index {}: {}",
                                   index_filename, index_ec);
          ec = index_ec;
          return;
        }
        assembly_to_cpg_index_in.emplace(assembly, index);
      }
    }
  }

  assembly_to_cpg_index = std::move(assembly_to_cpg_index_in);
  assembly_to_cpg_index_metadata = std::move(assembly_to_cpg_index_metadata_in);
}
