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

#include "methylome_metadata.hpp"

#include "automatic_json.hpp"  // for tag_invoke
#include "cpg_index.hpp"
#include "cpg_index_metadata.hpp"
#include "methylome_data.hpp"
#include "utilities.hpp"  // for get_time_as_string

#include <config.h>  // for VERSION

#include <boost/asio.hpp>  // boost::asio::ip::host_name();
// ADS: this one seems not needed
#include <boost/container_hash/hash.hpp>  // for hash_range
#include <boost/json.hpp>
#include <boost/system.hpp>  // for boost::system::error_code

#include <cerrno>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>  // std::move

[[nodiscard]] auto
methylome_metadata::init_env() -> std::error_code {
  // ADS: where are we getting comression status?
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
methylome_metadata::init(const cpg_index &index, const methylome_data &meth)
  -> std::tuple<methylome_metadata, std::error_code> {
  // ADS: (todo) should there be a better way to get the "compression"
  // status?
  boost::system::error_code boost_err;
  const auto host = boost::asio::ip::host_name(boost_err);
  if (boost_err)
    return {methylome_metadata{}, std::error_code{boost_err}};

  const auto index_hash = index.meta.index_hash;
  const auto methylome_hash = meth.hash();
  const auto assembly = index.meta.assembly;
  const auto n_cpgs = index.meta.n_cpgs;

  const auto [username, err] = get_username();
  if (err)
    return {methylome_metadata{}, err};

  static constexpr auto is_compressed_init = false;
  return {methylome_metadata{VERSION, host, username, get_time_as_string(),
                             methylome_hash, index_hash, assembly, n_cpgs,
                             is_compressed_init},
          std::error_code{}};
}

[[nodiscard]] auto
methylome_metadata::update(const methylome_data &meth) -> std::error_code {
  const auto err = init_env();
  if (err)
    return err;
  methylome_hash = meth.hash();
  return {};
}

[[nodiscard]] auto
methylome_metadata::read(const std::string &json_filename,
                         std::error_code &ec) -> methylome_metadata {
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

  methylome_metadata mm;
  boost::json::parse_into(mm, payload, ec);
  if (ec)
    return {};

  ec = std::error_code{};
  return mm;
}

[[nodiscard]]
static inline auto
make_methylome_metadata_filename(const std::string &dirname,
                                 const std::string &methylome_name) {
  const auto with_extension =
    std::format("{}{}", methylome_name, methylome_metadata::filename_extension);
  return (std::filesystem::path{dirname} / with_extension).string();
}

[[nodiscard]] auto
methylome_metadata::read(const std::string &dirname,
                         const std::string &methylome_name,
                         std::error_code &ec) -> methylome_metadata {
  return read(make_methylome_metadata_filename(dirname, methylome_name), ec);
}

[[nodiscard]] auto
methylome_metadata::write(const std::string &json_filename) const
  -> std::error_code {
  std::ofstream out(json_filename);
  if (!out)
    return std::make_error_code(std::errc(errno));
  if (!(out << boost::json::value_from(*this)))
    return std::make_error_code(std::errc(errno));
  return {};
}

[[nodiscard]] auto
methylome_metadata::tostring() const -> std::string {
  std::ostringstream o;
  if (!(o << boost::json::value_from(*this)))
    o.clear();
  return o.str();
}

[[nodiscard]] auto
get_default_methylome_metadata_filename(const std::string &methfile)
  -> std::string {
  return std::format("{}.json", methfile);
}
