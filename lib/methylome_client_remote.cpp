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

#include "methylome_client_remote.hpp"

#include "client_config.hpp"
#include "nlohmann/json.hpp"

#include <string>
#include <system_error>

namespace transferase {

[[nodiscard]] auto
methylome_client_remote::tostring_derived() const noexcept -> std::string {
  static constexpr auto n_indent = 4;
  nlohmann::json data = *this;
  return data.dump(n_indent);
}

auto
methylome_client_remote::load_methylome_name_list(
  const std::string &metadata_file, std::error_code &error) -> void {
  /// ADS: this is instantiating the transferase metadata object using the
  /// labels file, which for now has all the info needed.
  meta = methylome_name_list::read(metadata_file, error);
}

auto
methylome_client_remote::validate_derived(
  const bool require_metadata, std::error_code &error) noexcept -> void {
  // ADS: at this point we should check for existing files and directories?
  if (config.hostname.empty()) {
    error = methylome_client_remote_error_code::hostname_not_found;
    return;
  }
  if (config.port.empty()) {
    error = methylome_client_remote_error_code::port_not_found;
    return;
  }
  if (config.index_dir.empty()) {
    error = methylome_client_base_error_code::index_dir_not_found;
    return;
  }
  if (require_metadata && config.methylome_list.empty()) {
    error = methylome_client_base_error_code::methylome_name_list_not_found;
    return;
  }
}

}  // namespace transferase
