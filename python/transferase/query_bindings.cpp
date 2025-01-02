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

#include "query.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <format>

namespace py = pybind11;

PYBIND11_MODULE(query_container, m) {
  py::class_<xfrase::query_container>(m, "Query")
    // ADS: constructors only for testing
    .def(py::init<>())
    // bind const operator[] for read access (get)
    .def(
      "__getitem__",
      [](const xfrase::query_container &self, const std::size_t pos) {
        // Return tuple of two values
        const auto &elem = self[pos];
        return py::make_tuple(elem.first, elem.second);
      },
      py::arg("pos"))
    .def("__len__", &xfrase::size)
    // Define equality operator explicitly as returning a bool
    .def(
      "__eq__",
      [](const xfrase::query_container &a, const xfrase::query_container &b) {
        return (a <=> b) == std::strong_ordering::equal;
      },
      py::is_operator())
    .def("__repr__", [](const xfrase::query_container &q) {
      return std::format("<Query size={}>", q.v.size());
    });
}
