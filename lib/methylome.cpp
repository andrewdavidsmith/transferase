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

#include "genome_index.hpp"
#include "genome_index_metadata.hpp"
#include "methylome_data.hpp"
#include "methylome_metadata.hpp"

#include <cassert>
#include <filesystem>
#include <iterator>
#include <ranges>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace transferase {

[[nodiscard]] auto
methylome::are_valid_names(const std::vector<std::string> &methylome_names,
                           std::error_code &error) noexcept -> bool {
  if (!std::ranges::all_of(methylome_names, &is_valid_name)) {
    error = methylome_error_code::invalid_methylome_name;
    return false;
  }
  return true;
}

[[nodiscard]] auto
methylome::init_metadata(const genome_index &index) noexcept
  -> std::error_code {
  static constexpr auto is_compressed_init = false;
  if (std::size(data.cpgs) != index.meta.n_cpgs)
    return std::error_code{methylome_error_code::invalid_methylome_data};
  meta = {
    // clang-format off
    "",  // version
    "",  // host
    "",  // user
    "",  // creation_time
    data.hash(),
    index.meta.index_hash,
    index.meta.genome_name,
    index.meta.n_cpgs,
    is_compressed_init
    // clang-format on
  };
  return meta.init_env();
}

[[nodiscard]] auto
methylome::update_metadata() noexcept -> std::error_code {
  const std::error_code error = meta.init_env();
  if (error)
    return error;
  meta.methylome_hash = data.hash();
  return std::error_code{};
}

[[nodiscard]] auto
methylome::read(const std::string &dirname, const std::string &methylome_name,
                std::error_code &error) noexcept -> methylome {
  methylome m;
  m.meta = methylome_metadata::read(dirname, methylome_name, error);
  if (error)
    return {};
  m.data = methylome_data::read(dirname, methylome_name, m.meta, error);
  if (error)
    return {};
  return m;
}

auto
methylome::write(const std::string &outdir, const std::string &name,
                 std::error_code &error) const noexcept -> void {
  // make filenames
  const auto fn_wo_extn = std::filesystem::path{outdir} / name;
  const auto meta_filename = methylome_metadata::compose_filename(fn_wo_extn);
  const auto meta_write_ec = meta.write(meta_filename);
  if (meta_write_ec) {
    const auto meta_exists = std::filesystem::exists(meta_filename, error);
    if (!error && meta_exists) {
      [[maybe_unused]] const auto remove_ok =
        std::filesystem::remove(meta_filename, error);
    }
    error = meta_write_ec;  // 'error' takes value from attempted write
    return;
  }
  const auto data_filename = methylome_data::compose_filename(fn_wo_extn);
  const auto data_write_ec = data.write(data_filename, meta.is_compressed);
  if (data_write_ec) {
    const auto data_exists = std::filesystem::exists(data_filename, error);
    if (!error && data_exists) {
      [[maybe_unused]] const auto remove_ok =
        std::filesystem::remove(data_filename, error);
    }
    const auto meta_exists = std::filesystem::exists(meta_filename, error);
    if (!error && meta_exists) {
      std::error_code remove_ec;
      [[maybe_unused]] const auto remove_ok =
        std::filesystem::remove(meta_filename, remove_ec);
    }
    error = data_write_ec;  // 'error' takes value from attempted write
    return;
  }
  error = std::error_code{};  // 'error' passed by ref; clear it if ok here
  return;
}

[[nodiscard]] auto
methylome::files_exist(const std::string &directory,
                       const std::string &methylome_name) noexcept -> bool {
  const auto fn_wo_extn = std::filesystem::path{directory} / methylome_name;
  const auto meta_filename = methylome_metadata::compose_filename(fn_wo_extn);
  const auto data_filename = methylome_data::compose_filename(fn_wo_extn);

  std::error_code error;
  const auto meta_exists = std::filesystem::exists(meta_filename, error);
  if (error || !meta_exists)
    return false;
  const auto data_exists = std::filesystem::exists(data_filename, error);
  if (error || !data_exists)
    return false;
  return true;
}

[[nodiscard]] auto
methylome::list(const std::string &dirname,
                std::error_code &error) noexcept -> std::vector<std::string> {
  using dir_itr_type = std::filesystem::directory_iterator;
  auto dir_itr = dir_itr_type(dirname, error);
  if (error)
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

[[nodiscard]] auto
methylome::parse_methylome_name(const std::string &filename) noexcept
  -> std::string {
  auto s = std::filesystem::path{filename}.filename().string();
  const auto dot = s.find('.');
  return dot == std::string::npos ? s : s.replace(dot, std::string::npos, "");
}

/// @brief Get the genome information associated with the given methylome
/// name, without instantiating a methylome object.
[[nodiscard]] auto
methylome::get_genome_info(
  const std::string &methylome_dir, const std::string &methylome_name,
  std::error_code &error) noexcept -> std::tuple<std::string, std::uint64_t> {
  assert(!methylome_name.empty());
  const auto meta =
    methylome_metadata::read(methylome_dir, methylome_name, error);
  if (error)
    return {{}, {}};
  return {meta.genome_name, meta.index_hash};
}

}  // namespace transferase
