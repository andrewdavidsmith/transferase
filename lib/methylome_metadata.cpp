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

#include "environment_utilities.hpp"
#include "nlohmann/json.hpp"

#include <cerrno>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <system_error>
#include <tuple>  // for std::tie

namespace transferase {

[[nodiscard]] auto
methylome_metadata::init_env() -> std::error_code {
  std::error_code ec;
  std::tie(host, ec) = get_hostname();
  if (ec)
    return ec;
  std::tie(user, ec) = get_username();
  if (ec)
    return ec;
  version = get_version();
  creation_time = get_time_as_string();
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
  const nlohmann::json data = nlohmann::json::parse(in, nullptr, false);
  if (data.is_discarded()) {
    ec = std::make_error_code(std::errc::invalid_argument);
    return {};
  }
  methylome_metadata mm = data;
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
  if (!(out << tostring()))
    return std::make_error_code(std::errc(errno));
  return {};
}

[[nodiscard]] auto
methylome_metadata::tostring() const -> std::string {
  static constexpr auto n_indent = 4;
  nlohmann::json data = *this;
  return data.dump(n_indent);
}

}  // namespace transferase
