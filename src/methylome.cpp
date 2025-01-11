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

#include <filesystem>
#include <iterator>
#include <ranges>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace transferase {

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
    index.meta.assembly,
    index.meta.n_cpgs,
    is_compressed_init
    // clang-format on
  };
  // ADS: take care of variables not dependent on genome_index or
  // methylome_data
  const auto ec = meta.init_env();
  return ec;
}

[[nodiscard]] auto
methylome::update_metadata() noexcept -> std::error_code {
  const std::error_code ec = meta.init_env();
  if (ec)
    return ec;
  meta.methylome_hash = data.hash();
  return std::error_code{};
}

[[nodiscard]] auto
methylome::read(const std::string &dirname, const std::string &methylome_name,
                std::error_code &ec) noexcept -> methylome {
  methylome m;
  m.meta = methylome_metadata::read(dirname, methylome_name, ec);
  if (ec)
    return {};
  m.data = methylome_data::read(dirname, methylome_name, m.meta, ec);
  if (ec)
    return {};
  return m;
}

auto
methylome::write(const std::string &outdir, const std::string &name,
                 std::error_code &ec) const noexcept -> void {
  // make filenames
  const auto fn_wo_extn = std::filesystem::path{outdir} / name;
  const auto meta_filename = methylome_metadata::compose_filename(fn_wo_extn);
  const auto meta_write_ec = meta.write(meta_filename);
  if (meta_write_ec) {
    std::error_code ec;
    const auto meta_exists = std::filesystem::exists(meta_filename, ec);
    if (!ec && meta_exists) {
      std::error_code remove_ec;
      std::filesystem::remove(meta_filename, remove_ec);
    }
    ec = meta_write_ec;
    return;
  }
  const auto data_filename = methylome_data::compose_filename(fn_wo_extn);
  const auto data_write_ec = data.write(data_filename, meta.is_compressed);
  if (data_write_ec) {
    std::error_code ec;
    const auto data_exists = std::filesystem::exists(data_filename, ec);
    if (!ec && data_exists) {
      std::error_code remove_ec;
      std::filesystem::remove(data_filename, remove_ec);
    }
    const auto meta_exists = std::filesystem::exists(meta_filename, ec);
    if (!ec && meta_exists) {
      std::error_code remove_ec;
      std::filesystem::remove(meta_filename, remove_ec);
    }
  }
  ec = data_write_ec;
  return;
}

[[nodiscard]] auto
methylome::files_exist(const std::string &directory,
                       const std::string &methylome_name) noexcept -> bool {
  const auto fn_wo_extn = std::filesystem::path{directory} / methylome_name;
  const auto meta_filename = methylome_metadata::compose_filename(fn_wo_extn);
  const auto data_filename = methylome_data::compose_filename(fn_wo_extn);

  std::error_code ec;
  const auto meta_exists = std::filesystem::exists(meta_filename, ec);
  if (ec || !meta_exists)
    return false;
  const auto data_exists = std::filesystem::exists(data_filename, ec);
  if (ec || !data_exists)
    return false;
  return true;
}

[[nodiscard]] auto
methylome::list(const std::string &dirname,
                std::error_code &ec) noexcept -> std::vector<std::string> {
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

[[nodiscard]] auto
methylome::parse_methylome_name(const std::string &filename) noexcept
  -> std::string {
  auto s = std::filesystem::path{filename}.filename().string();
  const auto dot = s.find('.');
  return dot == std::string::npos ? s : s.replace(dot, std::string::npos, "");
}

}  // namespace transferase
