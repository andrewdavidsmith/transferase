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

#include <genome_index_data.hpp>
#include <genome_index_metadata.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>  // IWYU pragma: keep

namespace py = pybind11;

[[nodiscard]] inline auto
transferase_genome_index_read(const std::string &dirname,
                              const std::string &genome_name)
  -> transferase::genome_index {
  std::error_code ec;
  auto index = transferase::genome_index::read(dirname, genome_name, ec);
  if (ec)
    throw std::system_error(ec);
  return index;
}

[[nodiscard]] inline auto
transferase_genome_index_make_genome_index(const std::string &genome_file)
  -> transferase::genome_index {
  std::error_code ec;
  auto index = transferase::genome_index::make_genome_index(genome_file, ec);
  if (ec)
    throw std::system_error(ec);
  return index;
}

[[nodiscard]] inline auto
transferase_genome_index_parse_genome_name(const std::string &filename)
  -> std::string {
  std::error_code ec;
  auto genome_name = transferase::genome_index::parse_genome_name(filename, ec);
  if (ec)
    throw std::system_error(ec);
  return genome_name;
}

[[nodiscard]] inline auto
transferase_genome_index_list_genome_indexes(const std::string &directory)
  -> std::vector<std::string> {
  std::error_code ec;
  auto genome_indexes =
    transferase::genome_index::list_genome_indexes(directory, ec);
  if (ec)
    throw std::system_error(ec);
  return genome_indexes;
}

auto
genome_index_bindings(py::class_<transferase::genome_index> &cls) -> void {
  using namespace pybind11::literals;  // NOLINT
  cls.def(py::init<>())
    .def_readonly("data", &transferase::genome_index::data)
    .def_readonly("meta", &transferase::genome_index::meta)
    .def("is_consistent", &transferase::genome_index::is_consistent)
    .def("__hash__", &transferase::genome_index::get_hash)
    .def("__repr__", &transferase::genome_index::tostring)
    .def_static("read", &transferase_genome_index_read, "dirname"_a,
                "genome_name"_a)
    .def(
      "write",
      [](const transferase::genome_index &self, const std::string &outdir,
         const std::string &name) -> void {
        const std::error_code ec = self.write(outdir, name);
        if (ec)
          throw std::system_error(ec);
      },
      "outdir"_a, "name"_a)
    .def("make_query", &transferase::genome_index::make_query,
         py::arg("intervals").noconvert())
    .def_static(
      "make_genome_index", &transferase_genome_index_make_genome_index,
      "Create a genome index from a reference genome", "genome_file"_a)
    .def_static("files_exist", &transferase::genome_index::files_exist,
                "Check if genome index files exist in a directory.",
                "directory"_a, "genome_name"_a)
    .def_static(
      "parse_genome_name", &transferase_genome_index_parse_genome_name,
      "Parse the genome name from a FASTA format reference genome file.",
      "filename"_a)
    .def_static("list_genome_indexes",
                &transferase_genome_index_list_genome_indexes,
                "List all CpG indexes in a directory.", "directory"_a)
    //
    ;
}
