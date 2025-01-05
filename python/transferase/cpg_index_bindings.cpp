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

#include "cpg_index_bindings.hpp"

#include <cpg_index.hpp>
#include <genomic_interval.hpp>

#include <cpg_index_data.hpp>
#include <cpg_index_metadata.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>  // IWYU pragma: keep

namespace py = pybind11;

auto
cpg_index_bindings(py::class_<transferase::cpg_index> &cls) -> void {
  using namespace pybind11::literals;  // NOLINT
  cls.def(py::init<>())
    .def_readonly("data", &transferase::cpg_index::data)
    .def_readonly("meta", &transferase::cpg_index::meta)
    .def("is_consistent", &transferase::cpg_index::is_consistent)
    .def("__hash__", &transferase::cpg_index::get_hash)
    .def("__repr__", &transferase::cpg_index::tostring)
    .def_static("read", &transferase::cpg_index::read, py::arg("dirname"),
                py::arg("genome_name"), py::arg("error"))
    .def("write", &transferase::cpg_index::write, py::arg("outdir"), py::arg("name"))
    .def("make_query", &transferase::cpg_index::make_query, "intervals"_a)
    .def_static("make_cpg_index", &transferase::cpg_index::make_cpg_index,
                "Create a CpG index from a reference genome", "genome_file"_a,
                "error_code"_a)
    .def_static("files_exist", &transferase::cpg_index::files_exist,
                "Check if CpG index files exist in a directory.", "directory"_a,
                "genome_name"_a)
    .def_static(
      "parse_genome_name", &transferase::cpg_index::parse_genome_name,
      "Parse the genome name from a FASTA format reference genome file.",
      "filename"_a, "error_code"_a)
    .def_static("list_cpg_indexes", &transferase::cpg_index::list_cpg_indexes,
                "List all CpG indexes in a directory.", "directory"_a,
                "error_code"_a);
}
