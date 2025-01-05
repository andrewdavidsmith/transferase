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

#include "request_bindings.hpp"

#include <request.hpp>
#include <request_type_code.hpp>

#include <pybind11/pybind11.h>

#include <string>

namespace py = pybind11;

auto
request_type_code_bindings(py::enum_<transferase::request_type_code> &en) -> void {
  using namespace pybind11::literals;  // NOLINT
  // Enum binding for request_type_code
  en.value("intervals", transferase::request_type_code::intervals)
    .value("intervals_covered", transferase::request_type_code::intervals_covered)
    .value("bins", transferase::request_type_code::bins)
    .value("bins_covered", transferase::request_type_code::bins_covered)
    .value("n_request_types", transferase::request_type_code::n_request_types)
    .export_values();
}

auto
request_bindings(py::class_<transferase::request> &cls) -> void {
  using namespace pybind11::literals;  // NOLINT
  // Binding for request struct
  cls
    .def(py::init<>())  // Default constructor
    .def(py::init<std::string, transferase::request_type_code, std::uint64_t,
                  std::uint32_t>(),
         py::arg("methylome_name"), py::arg("request_type"),
         py::arg("index_hash"), py::arg("aux_value"))
    .def_readwrite("accession", &transferase::request::accession)
    .def_readwrite("request_type", &transferase::request::request_type)
    .def_readwrite("index_hash", &transferase::request::index_hash)
    .def_readwrite("aux_value", &transferase::request::aux_value)
    .def("n_intervals", &transferase::request::n_intervals)
    .def("bin_size", &transferase::request::bin_size)
    .def("__repr__", &transferase::request::summary)
    .def("is_valid_type", &transferase::request::is_valid_type)
    .def("is_intervals_request", &transferase::request::is_intervals_request)
    .def("is_bins_request", &transferase::request::is_bins_request)
    //
    ;
}
