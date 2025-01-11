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

#include "genome_index_metadata_bindings.hpp"

#include <genome_index_metadata.hpp>

#include <pybind11/pybind11.h>

#include <string>

namespace py = pybind11;

auto
genome_index_metadata_bindings(
  py::class_<transferase::genome_index_metadata> &cls) -> void {
  using namespace pybind11::literals;  // NOLINT
  cls.def(py::init<>())
    .def("init_env",
         [](transferase::genome_index_metadata &self) {
           const std::error_code ec = self.init_env();
           if (ec)
             throw std::system_error(ec);
         })
    .def("__repr__", &transferase::genome_index_metadata::tostring)
    .def("get_n_cpgs_chrom",
         &transferase::genome_index_metadata::get_n_cpgs_chrom)
    .def("get_n_bins", &transferase::genome_index_metadata::get_n_bins,
         "bin_size"_a)
    // bindings for the member variables
    .def_readwrite("version", &transferase::genome_index_metadata::version)
    .def_readwrite("host", &transferase::genome_index_metadata::host)
    .def_readwrite("user", &transferase::genome_index_metadata::user)
    .def_readwrite("creation_time",
                   &transferase::genome_index_metadata::creation_time)
    .def_readwrite("index_hash",
                   &transferase::genome_index_metadata::index_hash)
    .def_readwrite("assembly", &transferase::genome_index_metadata::assembly)
    .def_readwrite("n_cpgs", &transferase::genome_index_metadata::n_cpgs)
    // done
    ;
}
