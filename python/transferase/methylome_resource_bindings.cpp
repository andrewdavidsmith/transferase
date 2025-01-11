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

#include "methylome_resource_bindings.hpp"

#include <methylome_resource.hpp>

#include <pybind11/pybind11.h>

#include <cstdint>
#include <string>
#include <system_error>

#include <level_container.hpp>
#include <level_element.hpp>

namespace transferase {
struct query_container;
}  // namespace transferase

namespace py = pybind11;

auto
local_methylome_resource_bindings(
  py::class_<transferase::local_methylome_resource> &cls) -> void {
  using namespace pybind11::literals;  // NOLINT
  cls
    .def(py::init<const std::string &, const std::uint64_t>(), "directory"_a,
         "index_hash"_a)
    .def("__repr__", &transferase::local_methylome_resource::tostring)
    .def("get_levels",
         [](const transferase::local_methylome_resource &self,
            const std::string &methylome_name,
            const transferase::query_container &query) {
           std::error_code ec;
           auto lvls = self.get_levels(methylome_name, query, ec);
           if (ec)
             throw std::system_error(ec);
           return lvls;
         })
    .def("get_levels_covered",
         [](const transferase::local_methylome_resource &self,
            const std::string &methylome_name,
            const transferase::query_container &query) {
           std::error_code ec;
           auto lvls = self.get_levels_covered(methylome_name, query, ec);
           if (ec)
             throw std::system_error(ec);
           return lvls;
         })
    .def_readwrite("directory",
                   &transferase::local_methylome_resource::directory)
    .def_readwrite("index_hash",
                   &transferase::local_methylome_resource::index_hash)
    //
    ;
}

auto
remote_methylome_resource_bindings(
  py::class_<transferase::remote_methylome_resource> &cls) -> void {
  using namespace pybind11::literals;  // NOLINT
  cls
    .def(
      py::init<const std::string &, const std::string &, const std::uint64_t>(),
      "hostname"_a, "port_number"_a, "index_hash"_a)
    .def("__repr__", &transferase::remote_methylome_resource::tostring)
    .def("get_levels",
         [](const transferase::remote_methylome_resource &self,
            const std::string &methylome_name,
            const transferase::query_container &query) {
           std::error_code ec;
           auto lvls = self.get_levels(methylome_name, query, ec);
           if (ec)
             throw std::system_error(ec);
           return lvls;
         })
    .def("get_levels_covered",
         [](const transferase::remote_methylome_resource &self,
            const std::string &methylome_name,
            const transferase::query_container &query) {
           std::error_code ec;
           auto lvls = self.get_levels(methylome_name, query, ec);
           if (ec)
             throw std::system_error(ec);
           return lvls;
         })
    .def_readwrite("hostname",
                   &transferase::remote_methylome_resource::hostname)
    .def_readwrite("port_number",
                   &transferase::remote_methylome_resource::port_number)
    .def_readwrite("index_hash",
                   &transferase::remote_methylome_resource::index_hash)
    //
    ;
}
