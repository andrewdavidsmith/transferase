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

#include "genomic_interval_bindings.hpp"

#include <genome_index.hpp>
#include <genome_index_metadata.hpp>

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>  // IWYU pragma: keep

#include <format>
#include <functional>
#include <genomic_interval.hpp>
#include <string>
#include <vector>

namespace py = pybind11;

namespace transferase {

[[nodiscard]] inline auto
genomic_interval_read(const genome_index &index, const std::string &filename)
  -> std::vector<genomic_interval> {
  return genomic_interval::read(index, filename);
}

}  // namespace transferase

auto
genomic_interval_bindings(pybind11::class_<transferase::genomic_interval> &cls)
  -> void {
  using namespace pybind11::literals;  // NOLINT
  cls
    .def(py::init<>())
    // instance variables
    .def_readwrite("ch_id", &transferase::genomic_interval::ch_id,
                   "Numerical identifier for the chromosome")
    .def_readwrite("start", &transferase::genomic_interval::start,
                   "Start position of this interval in the chromosome")
    .def_readwrite("stop", &transferase::genomic_interval::stop,
                   "Stop position of this interval in the chromosome")
    // comparators all derived from operator<=> (allows different
    // types to be compared for equality or non-equality)
    // clang-format off
    .def(pybind11::self == pybind11::self)
    .def(pybind11::self != pybind11::self)
    .def(pybind11::self <  pybind11::self)
    .def(pybind11::self <= pybind11::self)
    .def(pybind11::self >  pybind11::self)
    .def(pybind11::self >= pybind11::self)
    // clang-format on
    .def(
      "__repr__",
      [](const transferase::genomic_interval &gi) {
        return std::format("{}", gi);
      },
      "Print a genomic interval with the numeric code for chromosome name")
    .def(
      "to_string",
      [](const transferase::genomic_interval &self,
         const transferase::genome_index &index) {
        return std::format("{}\t{}\t{}", index.meta.chrom_order.at(self.ch_id),
                           self.start, self.stop);
      },
      "Print a genomic interval with name of chromosome",
      py::arg("genome_index"))
    // static functions of genomic_interval class
    .def_static("read", &transferase::genomic_interval_read,
                "Read a BED file of genomic intervals file", "genome_index"_a,
                "filename"_a)
    .def_static("are_sorted", &transferase::genomic_interval::are_sorted,
                "Check if intervals are sorted", "intervals"_a)
    .def_static("are_valid",
                &transferase::genomic_interval::are_valid<
                  std::vector<transferase::genomic_interval>>,
                "Check if all intervals are valid (start <= stop)",
                "intervals"_a)
    //
    ;
}
