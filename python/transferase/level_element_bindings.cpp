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

#include "level_element_bindings.hpp"

#include <level_element.hpp>

#include <pybind11/pybind11.h>

#include <format>
#include <string>

namespace py = pybind11;

auto
level_element_bindings(py::class_<xfrase::level_element_t> &cls) -> void {
  cls.def(py::init<>())
    .def("__repr__",
         [](const xfrase::level_element_t &self) {
           return std::format("{} {}", self.n_meth, self.n_unmeth);
         })
    .def("json",
         [](const xfrase::level_element_t &self) {
           return std::format("{}", self);
         })
    .def_readwrite("n_meth", &xfrase::level_element_t::n_meth,
                   "Number of methylated observations")
    .def_readwrite("n_unmeth", &xfrase::level_element_t::n_unmeth,
                   "Number of unmethylated observations")
    //
    ;
}

auto
level_element_covered_bindings(py::class_<xfrase::level_element_covered_t> &cls)
  -> void {
  cls.def(py::init<>())
    .def("__repr__",
         [](const xfrase::level_element_covered_t &self) {
           return std::format("{} {} {}", self.n_meth, self.n_unmeth,
                              self.n_covered);
         })
    .def("json",
         [](const xfrase::level_element_covered_t &self) {
           return std::format("{}", self);
         })
    .def_readwrite("n_meth", &xfrase::level_element_covered_t::n_meth,
                   "Number of methylated observations")
    .def_readwrite("n_unmeth", &xfrase::level_element_covered_t::n_unmeth,
                   "Number of unmethylated observations")
    .def_readwrite("n_covered", &xfrase::level_element_covered_t::n_covered,
                   "Number of sites covered")
    //
    ;
}
