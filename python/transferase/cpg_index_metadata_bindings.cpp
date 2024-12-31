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

#include "cpg_index_metadata.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <format>
#include <string>
#include <system_error>
#include <tuple>

namespace py = pybind11;

PYBIND11_MODULE(cpg_index_metadata, m) {
  // Expose std::error_code as 'ErrorCode'
  py::class_<std::error_code>(m, "ErrorCode")
    .def(py::init<>())                          // Default constructor
    .def("value", &std::error_code::value)      // Get the error code value
    .def("message", &std::error_code::message)  // Get the error message
    .def("__repr__", [](const std::error_code &self) {
      return "<ErrorCode value: " + std::to_string(self.value()) +
             ", message: " + self.message() + ">";
    });

  py::class_<xfrase::cpg_index_metadata>(m, "CpgIndexMetadata")
    .def(py::init<>())  // Default constructor (if needed)

    .def_static("read",
                py::overload_cast<const std::string &, std::error_code &>(
                  &xfrase::cpg_index_metadata::read),
                py::arg("json_filename"), py::arg("ec"))
    .def_static(
      "read",
      py::overload_cast<const std::string &, const std::string &,
                        std::error_code &>(&xfrase::cpg_index_metadata::read),
      py::arg("dirname"), py::arg("genome_name"), py::arg("ec"))
    .def("write", &xfrase::cpg_index_metadata::write, py::arg("json_filename"))
    .def("init_env", &xfrase::cpg_index_metadata::init_env)
    .def("tostring", &xfrase::cpg_index_metadata::tostring)
    .def("get_n_cpgs_chrom", &xfrase::cpg_index_metadata::get_n_cpgs_chrom)
    .def("get_n_bins", &xfrase::cpg_index_metadata::get_n_bins,
         py::arg("bin_size"))

    // Bindings for the member variables
    .def_readwrite("version", &xfrase::cpg_index_metadata::version)
    .def_readwrite("host", &xfrase::cpg_index_metadata::host)
    .def_readwrite("user", &xfrase::cpg_index_metadata::user)
    .def_readwrite("creation_time", &xfrase::cpg_index_metadata::creation_time)
    .def_readwrite("index_hash", &xfrase::cpg_index_metadata::index_hash)
    .def_readwrite("assembly", &xfrase::cpg_index_metadata::assembly)
    .def_readwrite("n_cpgs", &xfrase::cpg_index_metadata::n_cpgs)
    .def_readwrite("chrom_index", &xfrase::cpg_index_metadata::chrom_index)
    .def_readwrite("chrom_order", &xfrase::cpg_index_metadata::chrom_order)
    .def_readwrite("chrom_size", &xfrase::cpg_index_metadata::chrom_size)
    .def_readwrite("chrom_offset", &xfrase::cpg_index_metadata::chrom_offset)
    .def_property_readonly_static("filename_extension", [](py::object) {
      return xfrase::cpg_index_metadata::filename_extension;
    });
}
