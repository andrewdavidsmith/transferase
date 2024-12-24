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

#include "cpg_index_metadata.hpp"

#include "utilities.hpp"  // for get_time_as_string

#include <config.h>  // for VERSION

#include <boost/asio.hpp>  // boost::asio::ip::host_name();
#include <boost/json.hpp>
#include <boost/system.hpp>  // for boost::system::error_code

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
#include <tuple>
#include <utility>  // for std::move
#include <vector>

[[nodiscard]] auto
cpg_index_metadata::init_env() -> std::error_code {
  boost::system::error_code boost_err;
  host = boost::asio::ip::host_name(boost_err);
  if (boost_err)
    return std::error_code{boost_err};

  const auto [username, err] = get_username();
  if (err)
    return err;

  version = VERSION;
  user = username;
  creation_time = get_time_as_string();

  return {};
}

[[nodiscard]] auto
cpg_index_metadata::get_n_bins(const std::uint32_t bin_size) const
  -> std::uint32_t {
  const auto get_n_bins_for_chrom = [&](const auto chrom_size) {
    return (chrom_size + bin_size) / bin_size;
  };
  return std::transform_reduce(std::cbegin(chrom_size), std::cend(chrom_size),
                               static_cast<std::uint32_t>(0), std::plus{},
                               get_n_bins_for_chrom);
}

[[nodiscard]] auto
cpg_index_metadata::tostring() const -> std::string {
  std::ostringstream o;
  if (!(o << boost::json::value_from(*this)))
    o.clear();
  return o.str();
}

[[nodiscard]] auto
cpg_index_metadata::get_n_cpgs_chrom() const -> std::vector<std::uint32_t> {
  std::vector<std::uint32_t> n_cpgs_chrom(chrom_offset);
  n_cpgs_chrom.push_back(n_cpgs);
  std::adjacent_difference(std::cbegin(n_cpgs_chrom), std::cend(n_cpgs_chrom),
                           std::begin(n_cpgs_chrom));
  std::shift_left(std::begin(n_cpgs_chrom), std::end(n_cpgs_chrom), 1);
  n_cpgs_chrom.resize(std::size(n_cpgs_chrom) - 1);
  return n_cpgs_chrom;
}

[[nodiscard]] auto
cpg_index_metadata::read(const std::string &json_filename)
  -> std::tuple<cpg_index_metadata, std::error_code> {
  std::ifstream in(json_filename);
  if (!in)
    return {cpg_index_metadata{}, std::make_error_code(std::errc(errno))};

  std::error_code ec;
  const auto filesize = std::filesystem::file_size(json_filename, ec);
  if (ec)
    return {cpg_index_metadata{}, ec};

  std::string payload(filesize, '\0');
  if (!in.read(payload.data(), filesize))
    return {cpg_index_metadata{}, std::make_error_code(std::errc(errno))};

  cpg_index_metadata cim;
  boost::json::parse_into(cim, payload, ec);
  if (ec)
    return {cpg_index_metadata{},
            cpg_index_metadata_error::failure_parsing_json};

  return {std::move(cim), {}};
}

[[nodiscard]] auto
cpg_index_metadata::write(const std::string &json_filename) const
  -> std::error_code {
  std::ofstream out(json_filename);
  if (!out)
    return std::make_error_code(std::errc(errno));
  if (!(out << boost::json::value_from(*this)))
    return std::make_error_code(std::errc(errno));
  return {};
}

[[nodiscard]] auto
get_default_cpg_index_metadata_filename(const std::string &indexfile)
  -> std::string {
  return std::format("{}.json", indexfile);
}
