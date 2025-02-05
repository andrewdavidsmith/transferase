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

#include "bindings_utils.hpp"

#include <remote_data_resource.hpp>  // for get_system_config_filename

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>  // IWYU pragma: keep

#include <filesystem>
#include <format>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace py = pybind11;

namespace transferase {

[[nodiscard]] auto
find_dir(const std::vector<std::string> &paths,
         const std::string &filename) -> std::string {
  for (const auto &p : paths) {
    // Some of the paths given by Python might not exist
    const auto path_elem_exists = std::filesystem::exists(p);
    if (!path_elem_exists)
      continue;
    const auto dir_itr = std::filesystem::recursive_directory_iterator(p);
    for (const auto &dir_entry : dir_itr) {
      const auto &curr_dir = dir_entry.path();
      if (std::filesystem::is_directory(curr_dir)) {
        const auto candidate = curr_dir / filename;
        if (std::filesystem::exists(candidate))
          return curr_dir.string();
      }
    }
  }
  throw std::runtime_error(
    std::format("Failed to locate system config file: {}", filename));
}

[[nodiscard]] auto
get_package_paths() -> std::vector<std::string> {
  py::object path = py::module::import("sys").attr("path");
  std::vector<std::string> paths;
  // cppcheck-suppress useStlAlgorithm
  for (const auto &p : path)
    paths.emplace_back(p.cast<std::string>());
  return paths;
}

[[nodiscard]] auto
find_system_config_dir() -> std::string {
  const auto sys_conf_file = get_system_config_filename();
  const auto package_paths = get_package_paths();
  return find_dir(package_paths, sys_conf_file);
}

}  // namespace transferase
