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

#ifndef PYTHON_TRANSFERASE_CLIENT_CONFIG_PYTHON_HPP_
#define PYTHON_TRANSFERASE_CLIENT_CONFIG_PYTHON_HPP_

#include <client_config.hpp>

#include <boost/describe.hpp>

#include <format>  // for std::vector??
#include <string>
#include <system_error>
#include <vector>  // IWYU pragma: keep

namespace transferase {

struct client_config_python : public client_config {
  auto
  configure_python_system_config(const std::vector<std::string> &genomes,
                                 const download_policy_t download_policy,
                                 const std::string &config_dir) const -> void;

  auto
  save_python(const std::string &directory) -> void;

  [[nodiscard]] static auto
  read_python(std::error_code &error) noexcept -> client_config_python;

  [[nodiscard]] static auto
  read_python() -> client_config_python {
    std::error_code error;
    auto obj = read_python(error);
    if (error)
      throw std::system_error(error);
    return obj;
  }
};

BOOST_DESCRIBE_STRUCT(client_config_python, (client_config), ())

}  // namespace transferase

#endif  // PYTHON_TRANSFERASE_CLIENT_CONFIG_PYTHON_HPP_
