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

#include "methylome_client.hpp"

#include "client_config.hpp"
#include "utilities.hpp"

#include <algorithm>
#include <cctype>  // for std::isgraph
#include <cerrno>  // for errno
#include <fstream>
#include <iterator>  // for std::size, std::begin, std::cend
#include <ranges>
#include <string>
#include <system_error>
#include <tuple>

namespace transferase {

[[nodiscard]] auto
methylome_client::get_default(std::error_code &error) noexcept
  -> methylome_client {
  const auto config = client_config::read(error);
  if (error)
    // error from get_config_file_default
    return {};

  methylome_client mc;
  mc.hostname = config.hostname;
  mc.port_number = config.port;

  return mc;
}

}  // namespace transferase
