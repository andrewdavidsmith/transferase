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

#include "genome_index_bindings.hpp"

#include <genome_index.hpp>
#include <genomic_interval.hpp>  // IWYU pragma: keep

#include "listobject.h"  // for PyList_New
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>  // IWYU pragma: keep
#include <nanobind/stl/vector.h>  // IWYU pragma: keep

#include <string>
#include <vector>

namespace nb = nanobind;

auto
genome_index_bindings(nb::class_<transferase::genome_index> &cls) -> void {
  using namespace nanobind::literals;  // NOLINT
  cls.def(nb::init<>())
    .def("is_consistent", &transferase::genome_index::is_consistent)
    .def("__hash__", &transferase::genome_index::get_hash)
    .def("__repr__", &transferase::genome_index::tostring)
    .def_static("read",
                nb::overload_cast<const std::string &, const std::string &>(
                  &transferase::genome_index::read),
                R"doc(
    Read a a GenomeIndex object from a directory.

    Parameters
    ----------

    directory (str): Directory where the genome_index files can be
        found.

    genome_name (str): Read the index for the genome with this name.
    )doc",
                "directory"_a, "genome_name"_a)
    .def("write",
         nb::overload_cast<const std::string &, const std::string &>(
           &transferase::genome_index::write, nb::const_),
         R"doc(
    Write this GenomeIndex to a directory.

    Parameters
    ----------

    directory (str): The directory in which to write the GenomeIndex.

    genome_name (str): The name of the genome; determines filenames
        written.
    )doc",
         "directory"_a, "name"_a)
    .def("make_query", &transferase::genome_index::make_query, R"doc(
    Construct a QueryContainer object for a given list of GenomicInterval
    objects.

    Parameters
    ----------

    intervals (list[GenomicInterval]): A list of GenomicInterval
        objects, assumed to be sorted within each chromosome.
    )doc",
         "intervals"_a)
    .def_static("make_genome_index",
                nb::overload_cast<const std::string &>(
                  &transferase::genome_index::make_genome_index),
                R"doc(
    Create a genome index from a reference genome.

    Parameters
    ----------

    genome_file (str): Filename for a reference genome in FASTA format
        (can be gzipped).
    )doc",
                "genome_file"_a)
    .def_static("files_exist", &transferase::genome_index::files_exist,
                R"doc(
    Check if genome index files exist in a directory.

    Parameters
    ----------

    directory (str): Directory to check.

    genome_name (str): Name of the genome to look for.
    )doc",
                "directory"_a, "genome_name"_a)
    .def_static("list_genome_indexes",
                nb::overload_cast<const std::string &>(
                  &transferase::genome_index::list_genome_indexes),
                R"doc(
    Get a list of names of all genome indexes in a directory.

    Parameters
    ----------

    directory (str): Directory to list.
    )doc",
                "directory"_a)
    //
    ;
}
