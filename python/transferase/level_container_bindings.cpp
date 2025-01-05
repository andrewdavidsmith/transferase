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

#include "level_container_bindings.hpp"

#include <level_container.hpp>
#include <level_element.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>  // IWYU pragma: keep

#include <cstddef>  // for std::size_t

namespace py = pybind11;

auto
level_container_bindings(
  py::class_<transferase::level_container<transferase::level_element_t>> &cls)
  -> void {
  cls
    .def(py::init<>())
    // Need to raise key error somehow
    .def(
      "__getitem__",
      [](const transferase::level_container<transferase::level_element_t> &self,
         const std::size_t pos) -> transferase::level_element_t const & {
        return self[pos];
      })
    // cppcheck-suppress-begin constParameterReference
    .def("__setitem__",
         [](transferase::level_container<transferase::level_element_t> &self,
            const std::size_t pos) { return self[pos]; })
    // cppcheck-suppress-end constParameterReference
    //
    ;
}

auto
level_container_covered_bindings(
  py::class_<transferase::level_container<transferase::level_element_covered_t>>
    &cls) -> void {
  cls
    .def(py::init<>())
    // Need to raise key error somehow
    .def(
      "__getitem__",
      [](
        const transferase::level_container<transferase::level_element_covered_t>
          &self,
        const std::size_t pos) -> transferase::level_element_covered_t const & {
        return self[pos];
      })
    // cppcheck-suppress-begin constParameterReference
    .def("__setitem__",
         [](transferase::level_container<transferase::level_element_covered_t>
              &self,
            const std::size_t pos) { return self[pos]; })
    // cppcheck-suppress-end constParameterReference
    //
    ;
}
