/* MIT License
 *
 * Copyright (c) 2024 Andrew D Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "genomic_interval_bindings.hpp"

#include <genome_index.hpp>
#include <genome_index_metadata.hpp>

#include <listobject.h>  // for PyList_New
#include <nanobind/nanobind.h>
#include <nanobind/operators.h>   // IWYU pragma: keep
#include <nanobind/stl/string.h>  // IWYU pragma: keep
#include <nanobind/stl/vector.h>  // IWYU pragma: keep

#include <compare>  // for std::operator<, std::operator<=
#include <format>
#include <genomic_interval.hpp>
#include <iterator>   // for std::ssize
#include <new>        // for operator new
#include <stdexcept>  // for std::out_of_range include <string>
#include <string>
#include <type_traits>  // for std::is_rvalue_reference_v, std...
#include <utility>      // for std::declval
#include <vector>

namespace nb = nanobind;

namespace transferase {

[[nodiscard]] inline auto
genomic_interval_read(const genome_index &index, const std::string &filename)
  -> std::vector<genomic_interval> {
  return genomic_interval::read(index, filename);
}

}  // namespace transferase

auto
genomic_interval_bindings(nanobind::class_<transferase::genomic_interval> &cls)
  -> void {
  using namespace nanobind::literals;  // NOLINT
  cls.def(nb::init<>())
    .def_rw("ch_id", &transferase::genomic_interval::ch_id,
            "Numerical identifier for the chromosome")
    .def_rw("start", &transferase::genomic_interval::start,
            R"doc(
    Start position of this interval in the chromosome
    )doc");
  cls.def_rw("stop", &transferase::genomic_interval::stop,
             R"doc(
     Stop position of this interval in the chromosome
     )doc");
  // cppcheck-suppress-begin duplicateExpression
  cls.def(nanobind::self == nanobind::self)
    .def(nanobind::self != nanobind::self)
    .def(nanobind::self < nanobind::self)
    .def(nanobind::self <= nanobind::self)
    .def(nanobind::self > nanobind::self)
    .def(nanobind::self >= nanobind::self);
  // cppcheck-suppress-end duplicateExpression
  cls
    .def(
      "__repr__",
      [](const transferase::genomic_interval &self) {
        const nb::tuple t = nb::make_tuple(self.ch_id, self.start, self.stop);
        return nb::repr(t);
      },
      R"doc(
    Print a genomic interval with the numeric code for chromosome name.
      )doc")
    .def(
      "to_string",
      [](const transferase::genomic_interval &self,
         const transferase::genome_index &index) {
        const auto n_chroms = std::ssize(index.meta.chrom_order);
        if (self.ch_id >= n_chroms)
          throw std::out_of_range(std::format(
            "Index out of range: ch_id={}, n_chroms={}", self.ch_id, n_chroms));
        const nb::tuple t = nb::make_tuple(index.meta.chrom_order[self.ch_id],
                                           self.start, self.stop);
        return nb::repr(t);
      },
      R"doc(
    Print a genomic interval with name of chromosome.

    Parameters
    ----------

    genome_index (GenomeIndex): Must correspond to the appropriate genome.
    )doc",
      "genome_index"_a)  // static functions of genomic_interval class
    .def_static("read", &transferase::genomic_interval_read,
                R"doc(
    Read a BED file of genomic intervals.

    Parameters
    ----------

    genome_index (GenomeIndex): Must correspond to the appropriate genome.

    filename (str): The name of the BED file.
    )doc",
                "genome_index"_a, "filename"_a)
    .def_static("are_sorted", &transferase::genomic_interval::are_sorted, R"doc(
    Check if intervals are sorted.

    Parameters
    ----------

    intervals (list[GenomicInterval]): The list of intervals to check.
    )doc",
                "intervals"_a)
    .def_static("are_valid",
                &transferase::genomic_interval::are_valid<
                  std::vector<transferase::genomic_interval>>,
                R"doc(
    Check if all intervals are valid (start <= stop).

    Parameters
    ----------

    intervals (list[GenomicInterval]): The list of intervals to check.
    )doc",
                "intervals"_a);
  cls.doc() = R"doc(
    A GenomicInterval is a representation of an interval in the genome as
    chromosome, start position and end position. These are half-open intervals
    and correspond to one line in a BED format file. The chromosome is
    represented as an integer identifier, but the chromosome name can be
    recovered using a GenomeIndex object, and formatted for printing with the
    to_string() method.
    )doc"
    //
    ;
}
