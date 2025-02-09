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

#include "client_config_python.hpp"

#include "bindings_utils.hpp"

#include <client_config.hpp>  // IWYU pragma: keep
#include <config_file_utils.hpp>
#include <logger.hpp>
#include <utilities.hpp>

#include <string>
#include <system_error>
#include <vector>

namespace transferase {

auto
client_config_python::configure_python_system_config(
  const std::vector<std::string> &genomes,
  const download_policy_t download_policy,
  const std::string &config_dir) const -> void {
  const auto sys_conf_dir = find_python_sys_config_dir();
  configure(genomes, download_policy, config_dir, sys_conf_dir);
}

auto
client_config_python::save_python(const std::string &directory) -> void {
  std::error_code error;
  save(directory, error);
  if (error)
    throw std::system_error(error);
}

[[nodiscard]] auto
client_config_python::read_python(std::error_code &error) noexcept
  -> client_config_python {
  const auto config_dir = get_default_config_dir(error);
  if (error)
    // error from get_default_config_dir
    return {};
  // Get the config filename
  const auto config_file = get_config_file(config_dir, error);
  if (error)
    return {};
  client_config_python config;
  parse_config_file(config, config_file, error);
  if (error)
    return client_config_python{};
  return config;
}

}  // namespace transferase
