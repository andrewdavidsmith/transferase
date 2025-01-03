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

#include <cpg_index.hpp>
#include <cpg_index_metadata.hpp>
#include <genomic_interval.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <algorithm>  // for std::ranges::all_of
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <system_error>  // for std::error_code
#include <vector>

namespace py = pybind11;

auto
genomic_interval_bindings(pybind11::class_<xfrase::genomic_interval> &cls)
  -> void {
  using namespace pybind11::literals;  // NOLINT
  // clang-format off
  cls.def(py::init<>())
    // instance variables
    .def_readwrite("ch_id", &xfrase::genomic_interval::ch_id,
                   "Identifier for this interval's chromosome")
    .def_readwrite("start", &xfrase::genomic_interval::start,
                   "Start position of this interval in the chromosome")
    .def_readwrite("stop", &xfrase::genomic_interval::stop,
                   "Stop position of this interval in the chromosome")
    // comparators all derived from operator<=> (should be easier)
    .def("__eq__", std::equal_to<xfrase::genomic_interval>{})
    .def("__ne__", std::not_equal_to<xfrase::genomic_interval>{})
    .def("__lt__", std::less<xfrase::genomic_interval>{})
    .def("__le__", std::less_equal<xfrase::genomic_interval>{})
    .def("__gt__", std::greater<xfrase::genomic_interval>{})
    .def("__ge__", std::greater_equal<xfrase::genomic_interval>{})
    .def("__repr__",
      [](const xfrase::genomic_interval &gi) { return std::format("{}", gi); })
    .def("to_string", [](const xfrase::genomic_interval &self,
                         const xfrase::cpg_index &index) {
      return std::format("{}\t{}\t{}", index.meta.chrom_order.at(self.ch_id),
                         self.start, self.stop);},
         "Print a genomic interval with name of chromosome",
         py::arg("cpg_index"))
    // static functions of genomic_interval class
    .def_static("read", &xfrase::genomic_interval::read,
                "Read a BED file of genomic intervals file", "cpg_index"_a,
                "filename"_a, "error_code"_a)
    .def_static("are_sorted", &xfrase::genomic_interval::are_sorted,
                "Check if intervals are sorted", "intervals"_a)
    .def_static("are_valid",
                &xfrase::genomic_interval::are_valid<std::vector<xfrase::genomic_interval>>,
                "Check if all intervals are valid (start <= stop)",
                "intervals"_a)
    // clang-format on
    ;
}
