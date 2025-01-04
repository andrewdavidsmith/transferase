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

#include <cpg_index.hpp>
#include <methylome.hpp>
#include <methylome_data.hpp>
#include <methylome_metadata.hpp>
#include <query_container.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <ranges>
#include <string>
#include <system_error>
#include <vector>

namespace py = pybind11;

auto
methylome_bindings(py::class_<xfrase::methylome> &cls) -> void {
  using namespace pybind11::literals;  // NOLINT
  cls.def(py::init<>())
    .def_static("read", &xfrase::methylome::read, "directory"_a,
                "methylome_name"_a, "error_code"_a)
    .def("is_consistent",
         [](const xfrase::methylome &self) -> bool {
           return self.is_consistent();
         })
    .def(
      "is_consistent",
      [](const xfrase::methylome &self, const xfrase::methylome &other) {
        return self.is_consistent(other);
      },
      "other"_a)
    .def("write", &xfrase::methylome::write, "output_directory"_a, "name"_a)
    .def("init_metadata", &xfrase::methylome::init_metadata, "index"_a)
    .def("update_metadata", &xfrase::methylome::update_metadata)
    .def("add", &xfrase::methylome::add, "other"_a)
    .def("__repr__", &xfrase::methylome::tostring)
    .def("get_levels",
         [](const xfrase::methylome &self, const xfrase::query_container &query) {
           return self.get_levels(query);
         })
    .def("get_levels_covered",
         [](const xfrase::methylome &self, const xfrase::query_container &query) {
           return self.get_levels_covered(query);
         })
    .def("get_levels",
         [](const xfrase::methylome &self, const std::uint32_t bin_size,
            const xfrase::cpg_index &index) {
           return self.get_levels(bin_size, index);
         })
    .def("get_levels_covered",
         [](const xfrase::methylome &self, const std::uint32_t bin_size,
            const xfrase::cpg_index &index) {
           return self.get_levels_covered(bin_size, index);
         })
    .def("global_levels", &xfrase::methylome::global_levels)
    .def("global_levels_covered", &xfrase::methylome::global_levels_covered);
}
