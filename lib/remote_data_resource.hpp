/* MIT License
 *
 * Copyright (c) 2025 Andrew D Smith
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

#ifndef LIB_REMOTE_DATA_RESOURCE_HPP_
#define LIB_REMOTE_DATA_RESOURCE_HPP_

#include "nlohmann/json.hpp"

#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <system_error>
#include <tuple>
#include <variant>  // for std::tuple
#include <vector>

namespace transferase {

struct remote_data_resources {
  std::string host;
  std::uint16_t port{};
  std::string path;

  [[nodiscard]] auto
  form_index_target_stem(const auto &genome) const {
    return (std::filesystem::path{path} / "indexes" / genome).string();
  }

  [[nodiscard]] auto
  form_metadata_target_stem(const auto &genome) const {
    return (std::filesystem::path{path} / "metadata/latest" / genome).string();
  }

  [[nodiscard]] auto
  get_metadata_target_stem() const {
    return (std::filesystem::path{path} / "metadata/latest/metadata").string();
  }

  [[nodiscard]] auto
  form_url(const auto &file) const {
    return std::format("{}:{}{}", host, port, file);
  }

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(remote_data_resources, host, port, path)
};

struct system_config {
  std::string hostname;
  std::uint16_t port{};
  std::vector<remote_data_resources> resources;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(system_config, hostname, port, resources)
};

[[nodiscard]] auto
get_system_config_filename() -> std::string;

[[nodiscard]] auto
get_transferase_server_info(std::error_code &error) noexcept
  -> std::tuple<std::string, std::uint16_t>;

#ifndef TRANSFERASE_NOEXCEPT
[[nodiscard]] inline auto
get_transferase_server_info() -> std::tuple<std::string, std::uint16_t> {
  std::error_code error;
  auto res = get_transferase_server_info(error);
  if (error)
    throw std::system_error(error);
  return res;
}
#endif

[[nodiscard]] auto
get_transferase_server_info(const std::string &data_dir,
                            std::error_code &error) noexcept
  -> std::tuple<std::string, std::uint16_t>;

#ifndef TRANSFERASE_NOEXCEPT
[[nodiscard]] inline auto
get_transferase_server_info(const std::string &data_dir)
  -> std::tuple<std::string, std::uint16_t> {
  std::error_code error;
  auto res = get_transferase_server_info(data_dir, error);
  if (error)
    throw std::system_error(error);
  return res;
}
#endif

[[nodiscard]] auto
get_remote_data_resources(std::error_code &error) noexcept
  -> std::vector<remote_data_resources>;

#ifndef TRANSFERASE_NOEXCEPT
[[nodiscard]] inline auto
get_remote_data_resources() -> std::vector<remote_data_resources> {
  std::error_code error;
  auto res = get_remote_data_resources(error);
  if (error)
    throw std::system_error(error);
  return res;
}
#endif

[[nodiscard]] auto
get_remote_data_resources(const std::string &data_dir,
                          std::error_code &error) noexcept
  -> std::vector<remote_data_resources>;

#ifndef TRANSFERASE_NOEXCEPT
[[nodiscard]] inline auto
get_remote_data_resources(const std::string &data_dir)
  -> std::vector<remote_data_resources> {
  std::error_code error;
  auto res = get_remote_data_resources(data_dir, error);
  if (error)
    throw std::system_error(error);
  return res;
}
#endif

}  // namespace transferase

template <>
struct std::formatter<transferase::remote_data_resources>
  : std::formatter<std::string> {
  auto
  format(const transferase::remote_data_resources &r,
         std::format_context &ctx) const {
    return std::format_to(ctx.out(), "{}:{}{}", r.host, r.port, r.path);
  }
};

#endif  // LIB_REMOTE_DATA_RESOURCE_HPP_
