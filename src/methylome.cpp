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

#include "methylome.hpp"

#include "methylome_data.hpp"
#include "methylome_metadata.hpp"

#include "logger.hpp"

#include <filesystem>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_set>
#include <utility>  // for std::move

[[nodiscard]] auto
methylome::is_consistent() const -> bool {
  static auto &lgr = logger::instance();

  lgr.debug("Methylome metadata indicates compressed: {}", meta.is_compressed);
  const auto n_cpgs_match = (meta.n_cpgs == data.get_n_cpgs());
  lgr.debug("Methylome number of cpgs match: {}", n_cpgs_match);
  const auto n_cpgs_match_ret = ((meta.is_compressed && !n_cpgs_match) ||
                                 (!meta.is_compressed && n_cpgs_match));

  const auto hashes_match = (meta.methylome_hash == data.hash());
  lgr.debug("Methylome hashes match: {}", hashes_match);
  const auto hashes_match_ret = ((meta.is_compressed && !hashes_match) ||
                                 (!meta.is_compressed && hashes_match));

  return n_cpgs_match_ret && hashes_match_ret;
}

[[nodiscard]] auto
read_methylome(const std::string &directory, const std::string &methylome_name,
               std::error_code &ec) -> methylome {
  const auto fn_wo_extn = std::filesystem::path{directory} / methylome_name;

  // ADS: metadata needs to be read first because it's the only way to
  // know if the methylome has been compressed...

  // compose methylome metadata filename
  const auto meta_filename = compose_methylome_metadata_filename(fn_wo_extn);
  methylome_metadata meta;
  std::tie(meta, ec) = methylome_metadata::read(meta_filename);
  if (ec)
    return {};

  // compose methylome data filename
  const auto data_filename = compose_methylome_data_filename(fn_wo_extn);
  methylome_data data;
  std::tie(data, ec) = methylome_data::read(data_filename, meta);
  if (ec)
    return {};

  return methylome{std::move(data), std::move(meta)};
}

[[nodiscard]] auto
methylome_files_exist(const std::string &directory,
                      const std::string &methylome_name) -> bool {
  const auto fn_wo_extn = std::filesystem::path{directory} / methylome_name;
  const auto meta_filename = compose_methylome_metadata_filename(fn_wo_extn);
  const auto data_filename = compose_methylome_data_filename(fn_wo_extn);
  return std::filesystem::exists(meta_filename) &&
         std::filesystem::exists(data_filename);
}

[[nodiscard]] auto
list_methylomes(const std::string &dirname,
                std::error_code &ec) -> std::vector<std::string> {
  static constexpr auto data_extn = methylome::data_extn;
  static constexpr auto meta_extn = methylome::meta_extn;

  using dir_itr_type = std::filesystem::directory_iterator;
  auto dir_itr = dir_itr_type(dirname, ec);
  if (ec)
    return {};

  const auto data_files = std::ranges::subrange{dir_itr, dir_itr_type{}} |
                          std::views::filter([&](const auto &d) {
                            return d.path().extension().string() == data_extn;
                          }) |
                          std::ranges::to<std::vector>();

  const auto to_meta_file = [&](const auto &d) {
    std::string tmp = d.path().filename().string();
    return tmp.replace(tmp.find('.'), std::string::npos, meta_extn);
  };

  auto meta_names = std::ranges::transform_view(data_files, to_meta_file) |
                    std::ranges::to<std::vector>();

  std::unordered_set<std::string> meta_names_lookup;
  for (const auto &m : meta_names)
    meta_names_lookup.emplace(m);
  meta_names.clear();

  const auto remove_all_suffixes = [&](std::string s) {
    const auto dot = s.find('.');
    return dot == std::string::npos ? s : s.replace(dot, std::string::npos, "");
  };

  for (const auto &d : dir_itr_type{dirname}) {
    const auto fn = d.path().filename().string();
    if (meta_names_lookup.contains(fn))
      meta_names.emplace_back(remove_all_suffixes(fn));
  }

  return meta_names;
}
