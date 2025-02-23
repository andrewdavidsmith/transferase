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

#ifndef LIB_SYSTEM_CONFIG_HPP_
#define LIB_SYSTEM_CONFIG_HPP_

#include "remote_data_resource.hpp"

#include "nlohmann/json.hpp"

#include <format>
#include <string>
#include <tuple>
#include <variant>  // for std::tuple
#include <vector>

namespace transferase {

struct system_config {
  std::string hostname;
  std::string port;
  std::vector<remote_data_resource> resources;

  [[nodiscard]] auto
  get_remote_resources() const -> const std::vector<remote_data_resource> & {
    return resources;
  }

  system_config() = default;
  explicit system_config(const std::string &sys_config_data_dir);

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(system_config, hostname, port, resources)
};

[[nodiscard]] auto
get_default_system_config_dirname() -> std::string;

[[nodiscard]] auto
get_system_config_filename() -> std::string;

[[nodiscard]] auto
get_transferase_server_info() -> std::tuple<std::string, std::string>;

[[nodiscard]] auto
get_transferase_server_info(const std::string &data_dir)
  -> std::tuple<std::string, std::string>;

// [[nodiscard]] auto
// get_remote_data_resource() -> std::vector<remote_data_resource>;

// [[nodiscard]] auto
// get_remote_data_resource(const std::string &data_dir)
//   -> std::vector<remote_data_resource>;

}  // namespace transferase

// template <>
// struct std::formatter<transferase::remote_data_resource>
//   : std::formatter<std::string> {
//   auto
//   format(const transferase::remote_data_resource &r,
//          std::format_context &ctx) const {
//     return std::format_to(ctx.out(), "{}:{}{}", r.tostring());
//   }
// };

#endif  // LIB_SYSTEM_CONFIG_HPP_
