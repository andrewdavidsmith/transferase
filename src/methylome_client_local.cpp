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
#include "config_file_utils.hpp"
#include "genome_index.hpp"
#include "genome_index_set.hpp"
#include "methylome_genome_map.hpp"
#include "utilities.hpp"

#include <boost/json.hpp>
#include <boost/mp11/algorithm.hpp>  // for boost::mp11::mp_for_each

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <iterator>  // for std::cbegin, std::cend
#include <memory>    // for std::make_shared
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace transferase {

[[nodiscard]] auto
methylome_client_local::tostring_derived() const noexcept -> std::string {
  std::ostringstream o;
  if (!(o << boost::json::value_from(*this)))
    o.clear();
  return o.str();
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
    error = methylome_client_base_error_code::metadata_not_found;
    return;
  }
}

}  // namespace transferase
