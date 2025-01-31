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

#include "methylome_server_bindings.hpp"

#include <level_element.hpp>
#include <methylome_server.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>  // IWYU pragma: keep

#include <cstdint>
#include <string>
#include <vector>

namespace transferase {
struct query_container;
template <typename level_element_type> struct level_container;
}  // namespace transferase

namespace py = pybind11;

auto
methylome_server_bindings(py::class_<transferase::methylome_server> &cls)
  -> void {
  using namespace pybind11::literals;  // NOLINT
  cls
    .def(
      py::init<const std::string &, const std::string &, const std::uint64_t>(),
      "hostname"_a, "port_number"_a, "index_hash"_a)
    .def("__repr__", &transferase::methylome_server::tostring)
    .def("get_levels",
         [](const transferase::methylome_server &self,
            const std::vector<std::string> &methylome_names,
            const transferase::query_container &query) {
           return self.get_levels<transferase::level_element_t>(methylome_names,
                                                                query);
         })
    .def("get_levels_covered",
         [](const transferase::methylome_server &self,
            const std::vector<std::string> &methylome_names,
            const transferase::query_container &query) {
           return self.get_levels<transferase::level_element_covered_t>(
             methylome_names, query);
         })
    .def_readwrite("hostname", &transferase::methylome_server::hostname)
    .def_readwrite("port_number", &transferase::methylome_server::port_number)
    .def_readwrite("index_hash", &transferase::methylome_server::index_hash)
    //
    ;
}
