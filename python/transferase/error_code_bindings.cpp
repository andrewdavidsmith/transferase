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

#include "error_code_bindings.hpp"

#include <pybind11/pybind11.h>

#include <format>
#include <string>
#include <system_error>

namespace py = pybind11;

auto
error_code_bindings(py::class_<std::error_code> &cls) -> void {
  cls.def(py::init<>())
    .def("value", &std::error_code::value)
    .def("message", &std::error_code::message)
    .def("__repr__",
         [](const std::error_code &self) {
           return std::format("<ErrorCode value: {}, message: {}>",
                              self.value(), self.message());
         })
    .def("__bool__",
         [](const std::error_code &self) { return self != std::error_code{}; })
    //
    ;
}
