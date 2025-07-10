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

#include "client_config.hpp"  // for filenames

#include "nlohmann/json.hpp"

#include <config.h>

#include <filesystem>
#include <format>
#include <string>
#include <tuple>
#include <variant>  // for std::tuple
#include <vector>

namespace transferase {

struct remote_data_resource {
  std::string hostname;
  std::string port;
  std::string path;

  auto
  operator<=>(const remote_data_resource &other) const = default;

  /// Used to identify both the remote url and local relative path for index
  /// files without the extension.
  [[nodiscard]] auto
  form_index_target_stem(const auto &genome) const {
    return (std::filesystem::path{path} / "indexes" / genome).string();
  }

  /// Used to identify both the remote url and local relative path for
  /// metadata table.
  [[nodiscard]] auto
  form_methbase_metadata_dataframe_target() const {
    return (std::filesystem::path{path} / "metadata" / "latest" /
            std::format(client_config::methbase_metadata_dataframe_default,
                        VERSION))
      .string();
  }

  /// Used to identify both the remote url and local relative path for the
  /// 'select' metadata file in JSON format. This might last because it seems
  /// reasonable to keep the visualization stuff separate.
  [[nodiscard]] auto
  form_select_metadata_target() const {
    return (std::filesystem::path{path} / "metadata" / "latest" /
            std::format(client_config::select_metadata_default, VERSION))
      .string();
  }

  /// The url is formed given a file path that includes a common directory
  /// expected to exist on the remote host as well as locally.
  [[nodiscard]] auto
  form_url(const auto &file_with_path) const {
    return std::format("{}:{}{}", hostname, port, file_with_path);
  }

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(remote_data_resource, hostname, port, path)
};

}  // namespace transferase

template <>
struct std::formatter<transferase::remote_data_resource>
  : std::formatter<std::string> {
  auto
  format(const transferase::remote_data_resource &r, auto &ctx) const {
    return std::formatter<std::string>::format(
      std::format("{}:{}{}", r.hostname, r.port, r.path), ctx);
  }
};

#endif  // LIB_REMOTE_DATA_RESOURCE_HPP_
