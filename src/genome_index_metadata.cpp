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

#include "genome_index_metadata.hpp"

#include "environment_utilities.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdint>  // for std::uint32_t
#include <filesystem>
#include <fstream>
#include <functional>  // for std::plus
#include <iterator>    // for std::cbegin
#include <numeric>     // for std::adjacent_difference
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>  // for std::tie, std::tuple
#include <vector>

namespace transferase {

[[nodiscard]] auto
genome_index_metadata::init_env() -> std::error_code {
  version = get_version();
  creation_time = get_time_as_string();
  std::error_code ec;
  std::tie(host, ec) = get_hostname();
  if (ec)
    return ec;
  std::tie(user, ec) = get_username();
  return ec;
}

[[nodiscard]] auto
genome_index_metadata::get_n_bins(const std::uint32_t bin_size) const
  -> std::uint32_t {
  const auto get_n_bins_for_chrom = [&](const auto chrom_size) {
    return (chrom_size + bin_size) / bin_size;
  };
  return std::transform_reduce(std::cbegin(chrom_size), std::cend(chrom_size),
                               static_cast<std::uint32_t>(0), std::plus{},
                               get_n_bins_for_chrom);
}

[[nodiscard]] auto
genome_index_metadata::tostring() const -> std::string {
  std::ostringstream o;
  if (!(o << boost::json::value_from(*this)))
    o.clear();
  return o.str();
}

[[nodiscard]] auto
genome_index_metadata::get_n_cpgs_chrom() const -> std::vector<std::uint32_t> {
  std::vector<std::uint32_t> n_cpgs_chrom(chrom_offset);
  n_cpgs_chrom.push_back(n_cpgs);
  std::adjacent_difference(std::cbegin(n_cpgs_chrom), std::cend(n_cpgs_chrom),
                           std::begin(n_cpgs_chrom));
  std::shift_left(std::begin(n_cpgs_chrom), std::end(n_cpgs_chrom), 1);
  n_cpgs_chrom.resize(std::size(n_cpgs_chrom) - 1);
  return n_cpgs_chrom;
}

[[nodiscard]] auto
genome_index_metadata::read(const std::string &json_filename,
                            std::error_code &ec) -> genome_index_metadata {
  std::ifstream in(json_filename);
  if (!in) {
    ec = std::make_error_code(std::errc(errno));
    return {};
  }

  const auto filesize = std::filesystem::file_size(json_filename, ec);
  if (ec)
    return {};

  std::string payload(filesize, '\0');
  if (!in.read(payload.data(), filesize)) {
    ec = std::make_error_code(std::errc(errno));
    return {};
  }

  genome_index_metadata meta;
  boost::json::parse_into(meta, payload, ec);
  if (ec) {
    ec = genome_index_metadata_error::failure_parsing_json;
    return {};
  }

  return meta;
}

[[nodiscard]]
static inline auto
make_genome_index_metadata_filename(const std::string &dirname,
                                    const std::string &genome_name) {
  const auto with_extension =
    std::format("{}{}", genome_name, genome_index_metadata::filename_extension);
  return (std::filesystem::path{dirname} / with_extension).string();
}

[[nodiscard]] auto
genome_index_metadata::read(const std::string &dirname,
                            const std::string &genome_name,
                            std::error_code &ec) -> genome_index_metadata {
  return read(make_genome_index_metadata_filename(dirname, genome_name), ec);
}

[[nodiscard]] auto
genome_index_metadata::write(const std::string &json_filename) const
  -> std::error_code {
  std::ofstream out(json_filename);
  if (!out)
    return std::make_error_code(std::errc(errno));
  if (!(out << boost::json::value_from(*this)))
    return std::make_error_code(std::errc(errno));
  return {};
}

}  // namespace transferase
