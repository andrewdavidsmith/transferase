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

#include "level_container_bindings.hpp"

#include <level_container_md.hpp>
#include <level_element.hpp>

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>     // IWYU pragma: keep
#include <nanobind/stl/string.h>  // IWYU pragma: keep
#include <nanobind/stl/tuple.h>   // IWYU pragma: keep
#include <nanobind/stl/vector.h>  // IWYU pragma: keep

#include <cstddef>  // for std::size_t
#include <cstdint>  // for std::uint32_t
#include <format>
#include <iterator>  // for std::size
#include <new>       // for operator new
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>  // for std::is_rvalue_reference_v, std::i...
#include <utility>      // for std::declval
#include <variant>      // for std::tuple
#include <vector>       // for std::vector

namespace nb = nanobind;

auto
level_container_bindings(
  nb::class_<transferase::level_container_md<transferase::level_element_t>>
    &cls) -> void {
  using namespace nanobind::literals;  // NOLINT
  using level_container_md =
    transferase::level_container_md<transferase::level_element_t>;
  cls.def(nb::init<>());
  cls.def("__len__", &level_container_md::size);
  cls.def_ro("n_rows", &level_container_md::n_rows);
  cls.def_ro("n_intervals", &level_container_md::n_rows);
  cls.def_ro("n_cols", &level_container_md::n_cols);
  cls.def_ro("n_methylomes", &level_container_md::n_cols);
  cls.def(
    "view_nparray",
    [](level_container_md &self) {
      using nparray = nb::ndarray<std::uint32_t, nb::numpy,
                                  nb::shape<-1, -1, 2>, nb::c_contig>;
      return nparray(self.v.data(), {self.n_cols, self.n_rows, 2}).cast();
    },
    nb::rv_policy::reference_internal);
  cls.def(
    "at",
    [](const level_container_md &self, const std::size_t i,
       const std::size_t j) -> std::tuple<std::uint32_t, std::uint32_t> {
      if (i >= self.n_rows || j >= self.n_cols)
        throw std::out_of_range("Index out of range");
      return std::make_tuple(self[i, j].n_meth, self[i, j].n_unmeth);
    },
    R"doc(
    Access the tuple (n_meth, n_unmeth) of numbers of methylated and
    unmethylated reads for the query interval corresponding to the given
    position in the container. These are returned by copy, so access times
    might differ for the get_n_meth and get_n_unmeth methods.

    Parameters
    ----------

    arg0 (int): The index of the query interval for which to get the numbers of
        methylated and unmethylated reads.

    arg1 (int): The index of the methylome for which to get the numbers of
        methylated and unmethylated reads.
    )doc");
  cls.def(
    "get_n_meth",
    [](const level_container_md &self, const std::size_t i,
       const std::size_t j) -> std::uint32_t {
      if (i >= self.n_rows || j >= self.n_cols)
        throw std::out_of_range("Index out of range");
      return self[i, j].n_meth;
    },
    R"doc(
    Access the number of methylated observations for the query interval
    corresponding to the given position.

    Parameters
    ----------

    arg0 (int): The index of the query interval for which to get the number of
        methylated reads.

    arg1 (int): The index of the methylome for which to get the number of
        methylated reads.
    )doc");
  cls.def(
    "get_n_unmeth",
    [](const level_container_md &self, const std::size_t i,
       const std::size_t j) -> std::uint32_t {
      if (i >= self.n_rows || j >= self.n_cols)
        throw std::out_of_range("Index out of range");
      return self[i, j].n_unmeth;
    },
    R"doc(
    Access the number of UNmethylated observations for the query interval
    corresponding to the given position.

    Parameters
    ----------

    arg0 (int): The index of the query interval for which to get the number of
        UNmethylated reads.

    arg1 (int): The index of the methylome for which to get the number of
        UNmethylated reads.
    )doc");
  cls.def(
    "get_wmean",
    [](const level_container_md &self, const std::size_t i,
       const std::size_t j) -> float {
      if (i >= self.n_rows || j >= self.n_cols)
        throw std::out_of_range("Index out of range");
      return self[i, j].get_wmean();
    },
    R"doc(
    Get the weighted mean methylation level for the interval corresponding to
    the given position, which is the number of methylated observations divided
    by the total number of observations.

    Parameters
    ----------

    arg0 (int): The index of the query interval for which to get the weighted
        mean methylation level.

    arg1 (int): The index of the methylome for which to get the weighted mean
        methylation level.
    )doc");
  cls.def(
    "all_wmeans",
    [](const level_container_md &self, const std::uint32_t min_reads) {
      using nparray =
        nb::ndarray<float, nb::numpy, nb::shape<-1, -1, 2>, nb::c_contig>;
      auto m = self.get_wmeans(min_reads);
      return nparray(m.data(), {self.n_cols, self.n_rows, 2}).cast();
    },
    nb::rv_policy::reference_internal,
    R"doc(
    Apply the 'get_wmean' function to all elements of this MLevels object,
    returning a list of weighted mean methylation levels. A value of -1.0
    means insufficient reads, but by default the minimium required reads is 0.

    Parameters
    ----------

    min_reads (int): The minimum number of reads below which the value will be
        given the value -1.0. Without specifying a value for this argument,
        intervals with no reads will result in a level of 0.0, which might be
        desired depending on your application.
    )doc",
    "min_reads"_a = 0u);
  cls.def("__str__", [](const level_container_md &self) -> std::string {
    return std::format("MLevels size={}", std::size(self));
  });
  cls.doc() = R"doc(
    A MLevels represents methylation levels in each among a list of
    GenomicInterval objects. This is the object type that is returned from a
    transferase query, unless you additionally request information about sites
    covered (see MLevelsCovered).
    )doc"
    //
    ;
}

auto
level_container_covered_bindings(
  nb::class_<
    transferase::level_container_md<transferase::level_element_covered_t>> &cls)
  -> void {
  using namespace nanobind::literals;  // NOLINT
  using level_container_md =
    transferase::level_container_md<transferase::level_element_covered_t>;
  cls.def(nb::init<>());
  cls.def("__len__", &level_container_md::size);
  cls.def_ro("n_rows", &level_container_md::n_rows);
  cls.def_ro("n_intervals", &level_container_md::n_rows);
  cls.def_ro("n_cols", &level_container_md::n_cols);
  cls.def_ro("n_methylomes", &level_container_md::n_cols);
  cls.def(
    "view_nparray",
    [](level_container_md &self) {
      using nparray = nb::ndarray<std::uint32_t, nb::numpy,
                                  nb::shape<-1, -1, 3>, nb::c_contig>;
      return nparray(self.v.data(), {self.n_cols, self.n_rows, 3}).cast();
    },
    nb::rv_policy::reference_internal);
  cls.def(
    "at",
    [](const level_container_md &self, const std::size_t i, const std::size_t j)
      -> std::tuple<std::uint32_t, std::uint32_t, std::uint32_t> {
      if (i >= self.n_rows || j >= self.n_cols)
        throw std::out_of_range("Index out of range");
      const auto &x = self[i, j];
      return std::make_tuple(x.n_meth, x.n_unmeth, x.n_covered);
    },
    R"doc(
    Access the tuple (n_meth, n_unmeth, n_covered) of numbers of methylated
    and unmethylated reads, along with number of sites with at least one read,
    for the interval corresponding to the given position in the container.
    These are returned by copy, so access times might differ for the
    get_n_meth and get_n_unmeth methods.

    Parameters
    ----------

    arg0 (int): The index of the query interval for which to get the numbers
        of methylated and unmethylated reads.

    arg1 (int): The index of the methylome for which to get the numbers of
        methylated and unmethylated reads.
    )doc");
  cls.def(
    "get_n_meth",
    [](const level_container_md &self, const std::size_t i,
       const std::size_t j) -> std::uint32_t {
      if (i >= self.n_rows || j >= self.n_cols)
        throw std::out_of_range("Index out of range");
      return self[i, j].n_meth;
    },
    R"doc(
    Access the number of methylated observations for the query interval
    corresponding to the given position.

    Parameters
    ----------

    arg0 (int): The index of the query interval for which to get the number of
        methylated reads.

    arg1 (int): The index of the methylome for which to get the number of
        methylated reads.
    )doc");
  cls.def(
    "get_n_unmeth",
    [](const level_container_md &self, const std::size_t i,
       const std::size_t j) -> std::uint32_t {
      if (i >= self.n_rows || j >= self.n_cols)
        throw std::out_of_range("Index out of range");
      return self[i, j].n_unmeth;
    },
    R"doc(
    Access the number of UNmethylated observations for the query interval
    corresponding to the given position.

    Parameters
    ----------

    arg0 (int): The index of the query interval for which to get the number of
        UNmethylated reads.

    arg1 (int): The index of the methylome for which to get the number of
        UNmethylated reads.
    )doc");
  cls.def(
    "get_n_covered",
    [](const level_container_md &self, const std::size_t i,
       const std::size_t j) -> std::uint32_t {
      if (i >= self.n_rows || j >= self.n_cols)
        throw std::out_of_range("Index out of range");
      return self[i, j].n_covered;
    },
    R"doc(
    Access the number of covered sites in the interval corresponding to the
    given position.

    Parameters
    ----------

    arg0 (int): The index of the query interval for which to get the number of
        covered sites.

    arg1 (int): The index of the methylome for which to get the number of
        covered sites.
    )doc");
  cls.def(
    "get_wmean",
    [](const level_container_md &self, const std::size_t i,
       const std::size_t j) -> double {
      if (i >= self.n_rows || j >= self.n_cols)
        throw std::out_of_range("Index out of range");
      return self[i, j].get_wmean();
    },
    R"doc(
    Get the weighted mean methylation level for the interval corresponding to
    the given position, which is the number of methylated observations divided
    by the total number of observations.

    Parameters
    ----------

    arg0 (int): The index of the query interval for which to get the weighted
        mean methylation level.

    arg1 (int): The index of the methylome for which to get the weighted mean
        methylation level.
    )doc");
  cls.def(
    "all_wmeans",
    [](const level_container_md &self, const std::uint32_t min_reads) {
      using nparray =
        nb::ndarray<float, nb::numpy, nb::shape<-1, -1, 2>, nb::c_contig>;
      auto m = self.get_wmeans(min_reads);
      return nparray(m.data(), {self.n_cols, self.n_rows, 2}).cast();
    },
    nb::rv_policy::reference_internal,
    R"doc(
    Apply the 'get_wmean' function to all elements of this MLevelsCovered
    object, returning a list of weighted mean methylation levels. A value of
    -1.0 means insufficient reads, but by default the minimium required reads
    is 0.

    Parameters
    ----------

    min_reads (int): The minimum number of reads below which the value will be
        given the value -1.0. Without specifying a value for this argument,
        intervals with no reads will result in a level of 0.0, which might be
        desired depending on your application.
    )doc",
    "min_reads"_a = 0u);
  cls
    .def("__str__",
         [](const level_container_md &self) -> std::string {
           return std::format("MLevelsCovered size={}", std::size(self));
         })
    .doc() = R"doc(
    A MLevelsCovered represents methylation levels in each among a list of
    GenomicInterval objects. This is the object type that is returned from a
    transferase query if you request information about sites covered.
    )doc"
    //
    ;
}
