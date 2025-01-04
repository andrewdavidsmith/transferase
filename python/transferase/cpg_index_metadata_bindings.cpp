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

#include "cpg_index_metadata_bindings.hpp"

#include <cpg_index_metadata.hpp>

#include <pybind11/pybind11.h>

#include <string>

namespace py = pybind11;

auto
cpg_index_metadata_bindings(py::class_<xfrase::cpg_index_metadata> &cls)
  -> void {
  using namespace pybind11::literals;  // NOLINT
  cls.def(py::init<>())
    .def("init_env", &xfrase::cpg_index_metadata::init_env)
    .def("__repr__", &xfrase::cpg_index_metadata::tostring)
    .def("get_n_cpgs_chrom", &xfrase::cpg_index_metadata::get_n_cpgs_chrom)
    .def("get_n_bins", &xfrase::cpg_index_metadata::get_n_bins, "bin_size"_a)
    // bindings for the member variables
    .def_readwrite("version", &xfrase::cpg_index_metadata::version)
    .def_readwrite("host", &xfrase::cpg_index_metadata::host)
    .def_readwrite("user", &xfrase::cpg_index_metadata::user)
    .def_readwrite("creation_time", &xfrase::cpg_index_metadata::creation_time)
    .def_readwrite("index_hash", &xfrase::cpg_index_metadata::index_hash)
    .def_readwrite("assembly", &xfrase::cpg_index_metadata::assembly)
    .def_readwrite("n_cpgs", &xfrase::cpg_index_metadata::n_cpgs)
    // done
    ;
}
