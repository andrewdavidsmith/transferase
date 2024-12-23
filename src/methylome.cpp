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
#include "xfrase_error.hpp"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdint>  // for uint32_t, uint16_t, uint8_t, uint64_t
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>  // for is_same
#include <utility>      // for pair, move
#include <vector>

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
