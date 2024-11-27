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

#include "cpg_index_meta.hpp"

#include "cpg_index.hpp"
#include "utilities.hpp"

#include <config.h>

#include <boost/asio.hpp>  // boost::asio::ip::host_name();
#include <boost/json.hpp>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>

[[nodiscard]] auto
cpg_index_meta::write(const cpg_index_meta &cm,
                      const std::string &json_filename) -> std::error_code {
  std::ofstream out(json_filename);
  if (!out)
    return std::make_error_code(std::errc(errno));
  if (!(out << boost::json::value_from(cm)))
    return std::make_error_code(std::errc(errno));
  return {};
}

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
cpg_index_meta::tostring() const -> std::string {
  std::ostringstream o;
  if (!(o << boost::json::value_from(*this)))
    o.clear();
  return o.str();
}

[[nodiscard]] auto
cpg_index_meta::init(const std::string &index_filename)
  -> std::tuple<cpg_index_meta, std::error_code> {
  // ADS: (todo) should there be a better way to get the "compression"
  // status?
  cpg_index_meta cm;
  std::error_code err;
  cm.index_hash = get_adler(index_filename, err);
  if (err)
    return {{}, err};

  boost::system::error_code boost_err;
  cm.host = boost::asio::ip::host_name(boost_err);
  if (boost_err)
    return {{}, err};

  cm.assembly = get_assembly_from_filename(index_filename, err);
  if (err)
    return {{}, err};

  std::tie(cm.user, err) = get_username();

  cm.version = VERSION;
  cm.creation_time = get_time_as_string();

  cpg_index ci;
  err = ci.read(index_filename);

  cm.n_cpgs = ci.n_cpgs_total;
  cm.chrom_order = ci.chrom_order;
  cm.chrom_index = ci.chrom_index;
  cm.chrom_size = ci.chrom_size;
  cm.chrom_offset = ci.chrom_offset;

  return {cm, err};
}

[[nodiscard]] auto
cpg_index_meta::read(const std::string &json_filename)
  -> std::tuple<cpg_index_meta, std::error_code> {
  std::ifstream in(json_filename);
  if (!in)
    return {cpg_index_meta{}, std::make_error_code(std::errc(errno))};

  std::error_code ec;
  const auto filesize = std::filesystem::file_size(json_filename, ec);
  if (ec)
    return {cpg_index_meta{}, ec};

  std::string payload(filesize, '\0');
  if (!in.read(payload.data(), filesize))
    return {cpg_index_meta{}, std::make_error_code(std::errc(errno))};

  cpg_index_meta cim;
  boost::json::parse_into(cim, payload, ec);
  if (ec)
    return {cpg_index_meta{}, cpg_index_meta_error::failure_parsing_json};

  return {cim, cpg_index_meta_error::ok};
}

[[nodiscard]] auto
get_default_cpg_index_meta_filename(const std::string &indexfile)
  -> std::string {
  return std::format("{}.json", indexfile);
}
