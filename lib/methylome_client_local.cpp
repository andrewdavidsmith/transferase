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

#include "methylome_client_local.hpp"

#include "client_config.hpp"

#include "nlohmann/json.hpp"

#include <iterator>  // std::size
#include <string>
#include <system_error>
#include <tuple>
#include <vector>

namespace transferase {

[[nodiscard]] auto
methylome_client_local::tostring_derived() const noexcept -> std::string {
  static constexpr auto n_indent = 4;
  nlohmann::json data = *this;
  return data.dump(n_indent);
}

auto
methylome_client_local::validate_derived(std::error_code &error) noexcept
  -> void {
  if (config.methylome_dir.empty()) {
    error =
      methylome_client_local_error_code::methylome_dir_not_found_in_config;
    return;
  }
  if (config.index_dir.empty()) {
    error = methylome_client_base_error_code::index_dir_not_found;
    return;
  }
  if (config.metadata_file.empty()) {
    error = methylome_client_base_error_code::transferase_metadata_not_found;
    return;
  }
}

[[nodiscard]] auto
methylome_client_local::get_genome_and_index_hash(
  const std::vector<std::string> &methylome_names, std::error_code &error)
  const noexcept -> std::tuple<std::string, std::uint64_t> {
  assert(!config.methylome_dir.empty());
  const auto [genome, index_hash] = methylome::get_genome_info(
    config.methylome_dir, methylome_names.front(), error);
  if (error)
    return {{}, {}};
  for (auto i = 1u; i < std::size(methylome_names); ++i) {
    const auto [curr_genome, curr_index_hash] = methylome::get_genome_info(
      config.methylome_dir, methylome_names[i], error);
    if (error)
      return {{}, {}};
    if (index_hash != curr_index_hash) {
      error =
        methylome_client_local_error_code::inconsistent_methylome_metadata;
      return {{}, {}};
    }
  }
  return {genome, index_hash};
}

}  // namespace transferase
