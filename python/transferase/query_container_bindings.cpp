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

#include "query_container_bindings.hpp"

#include <query_container.hpp>

#include <pybind11/operators.h>  // IWYU pragma: keep
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>  // IWYU pragma: keep

#include <cstdint>  // for std::uint32_t
#include <format>
#include <string>

namespace py = pybind11;

auto
query_container_bindings(py::class_<transferase::query_container> &cls)
  -> void {
  cls.def(py::init<>())
    .def("__len__",
         [](const transferase::query_container &self) -> std::uint32_t {
           return transferase::size(self);
         })
    .def(pybind11::self == pybind11::self)
    .def(pybind11::self != pybind11::self)
    .def("__repr__",
         [](const transferase::query_container &self) -> std::string {
           return std::format("<QueryContainer size={}>",
                              transferase::size(self));
         })
    .doc() = R"doc(

    A QueryContainer is a representation for a list of GenomicInterval
    objects that has been packaged for use in a transferase query. You
    can't do anything else with a QueryContainer, but it allows you to
    avoid repeating some of the work if you want to use the same set
    of GenomicIntervals in more than one query.

    )doc"
    //
    ;
}
