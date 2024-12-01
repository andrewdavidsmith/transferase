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

#include "automatic_json.hpp"
#include "cpg_index_meta.hpp"  // get_assembly_from_filename
#include "methylome.hpp"

#include <config.h>

#include <boost/asio.hpp>  // boost::asio::ip::host_name();
#include <boost/json.hpp>

#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <print>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>  // std::move

[[nodiscard]] static auto
get_username() -> std::tuple<std::string, std::error_code> {
  static constexpr auto username_maxsize = 128;
  std::array<char, username_maxsize> buf;
  if (getlogin_r(buf.data(), username_maxsize))
    return {std::string(), std::make_error_code(std::errc(errno))};
  return {std::string(buf.data()), {}};
}

[[nodiscard]] static auto
get_time_as_string() -> std::string {
  const auto now{std::chrono::system_clock::now()};
  const std::chrono::year_month_day ymd{
    std::chrono::floor<std::chrono::days>(now)};
  const std::chrono::hh_mm_ss hms{
    std::chrono::floor<std::chrono::seconds>(now) -
    std::chrono::floor<std::chrono::days>(now)};
  return std::format("{:%F} {:%T}", ymd, hms);
}

[[nodiscard]] auto
methylome_metadata::init(const cpg_index_meta &cim, const methylome &meth,
                         const bool is_compressed)
  -> std::tuple<methylome_metadata, std::error_code> {
  // ADS: (todo) should there be a better way to get the "compression"
  // status?

  boost::system::error_code boost_err;
  const auto host = boost::asio::ip::host_name(boost_err);
  if (boost_err)
    return {{}, std::error_code{boost_err}};

  const auto index_hash = cim.index_hash;
  const auto methylome_hash = meth.hash();
  const auto assembly = cim.assembly;
  const auto n_cpgs = cim.n_cpgs;

  const auto [username, err] = get_username();
  if (err)
    return {{}, err};

  return {methylome_metadata{VERSION, host, username, get_time_as_string(),
                             methylome_hash, index_hash, assembly, n_cpgs,
                             is_compressed},
          std::error_code{}};
}

[[nodiscard]] auto
methylome_metadata::update(const methylome &meth) -> std::error_code {
  version = VERSION;
  boost::system::error_code boost_err;
  host = boost::asio::ip::host_name(boost_err);
  if (boost_err)
    return boost_err;
  std::error_code err;
  std::tie(user, err) = get_username();
  if (err)
    return err;
  creation_time = get_time_as_string();
  methylome_hash = meth.hash();
  return {};
}

[[nodiscard]] auto
methylome_metadata::read(const std::string &json_filename)
  -> std::tuple<methylome_metadata, std::error_code> {
  std::ifstream in(json_filename);
  if (!in)
    return {methylome_metadata{}, std::make_error_code(std::errc(errno))};

  std::error_code ec;
  const auto filesize = std::filesystem::file_size(json_filename, ec);
  if (ec)
    return {methylome_metadata{}, ec};

  std::string payload(filesize, '\0');
  if (!in.read(payload.data(), filesize))
    return {{}, std::make_error_code(std::errc(errno))};

  methylome_metadata mm;
  boost::json::parse_into(mm, payload, ec);
  if (ec)
    return {methylome_metadata{},
            methylome_metadata_error::failure_parsing_json};

  return {std::move(mm), methylome_metadata_error::ok};
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
  return std::format("version: {}\n"
                     "host: {}\n"
                     "user: {}\n"
                     "creation_time: \"{}\"\n"
                     "methylome_hash: {}\n"
                     "index_hash: {}\n"
                     "assembly: {}\n"
                     "n_cpgs: {}",
                     version, host, user, creation_time, methylome_hash,
                     index_hash, assembly, n_cpgs);
}

auto
get_default_methylome_metadata_filename(const std::string &methfile)
  -> std::string {
  return std::format("{}.json", methfile);
}
