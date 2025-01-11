/* MIT License
 *
 * Copyright (c) 2024 Andrew D Smith
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

#include "methylome_bindings.hpp"

#include <genome_index.hpp>  // IWYU pragma: keep
#include <methylome.hpp>

#include <pybind11/pybind11.h>

#include <cstdint>

namespace transferase {
struct level_element_covered_t;
struct level_element_t;
struct query_container;
template <typename level_element_type> struct level_container;
}  // namespace transferase

namespace py = pybind11;

[[nodiscard]] static inline auto
transferase_methylome_read(const std::string &directory,
                           const std::string &methylome_name)
  -> transferase::methylome {
  std::error_code ec;
  auto meth = transferase::methylome::read(directory, methylome_name, ec);
  if (ec)
    throw std::system_error(ec, std::format("directory={}, methylome_name={}",
                                            directory, methylome_name));
  return meth;
}

auto
methylome_bindings(py::class_<transferase::methylome> &cls) -> void {
  using namespace pybind11::literals;  // NOLINT
  cls.def(py::init<>())
    .def_static("read", &transferase_methylome_read, "directory"_a,
                "methylome_name"_a)
    .def("is_consistent",
         [](const transferase::methylome &self) -> bool {
           return self.is_consistent();
         })
    .def(
      "is_consistent",
      [](const transferase::methylome &self,
         const transferase::methylome &other) -> bool {
        return self.is_consistent(other);
      },
      "other"_a)
    .def(
      "write",
      [](const transferase::methylome &self, const std::string &directory,
         const std::string &name) -> void {
        const std::error_code ec = self.write(directory, name);
        if (ec)
          throw std::system_error(ec);
      },
      "directory"_a, "name"_a)
    .def(
      "init_metadata",
      [](transferase::methylome &self,
         const transferase::genome_index &index) -> void {
        const std::error_code ec = self.init_metadata(index);
        if (ec)
          throw std::system_error(ec);
      },
      "index"_a)
    .def("update_metadata",
         [](transferase::methylome &self) -> void {
           const std::error_code ec = self.update_metadata();
           if (ec)
             throw std::system_error(ec);
         })
    .def("add", &transferase::methylome::add, "other"_a)
    .def("__repr__", &transferase::methylome::tostring)
    .def("get_levels",
         [](const transferase::methylome &self,
            const transferase::query_container &query) {
           return self.get_levels(query);
         })
    .def("get_levels_covered",
         [](const transferase::methylome &self,
            const transferase::query_container &query) {
           return self.get_levels_covered(query);
         })
    .def("get_levels",
         [](const transferase::methylome &self, const std::uint32_t bin_size,
            const transferase::genome_index &index) {
           return self.get_levels(bin_size, index);
         })
    .def("get_levels_covered",
         [](const transferase::methylome &self, const std::uint32_t bin_size,
            const transferase::genome_index &index) {
           return self.get_levels_covered(bin_size, index);
         })
    .def("global_levels", &transferase::methylome::global_levels)
    .def("global_levels_covered",
         &transferase::methylome::global_levels_covered);
}
