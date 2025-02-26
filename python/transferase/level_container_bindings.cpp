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

#include <level_container.hpp>
#include <level_element.hpp>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>

#include <cstddef>  // for std::size_t
#include <cstdint>  // for std::uint32_t
#include <format>
#include <stdexcept>
#include <string>
#include <tuple>
#include <variant>  // for std::tuple

namespace nb = nanobind;

auto
level_container_bindings(
  nb::class_<transferase::level_container<transferase::level_element_t>> &cls)
  -> void {
  using namespace nanobind::literals;  // NOLINT
  namespace xfr = transferase;
  cls.def(nb::init<>());
  cls.def("__len__",
          [](const xfr::level_container<xfr::level_element_t> &self) {
            return std::size(self);
          });
  cls.def(
    "__getitem__",
    [](const xfr::level_container<xfr::level_element_t> &self,
       const std::size_t pos) -> std::tuple<std::uint32_t, std::uint32_t> {
      if (pos >= std::size(self))
        throw std::out_of_range("Index out of range");
      return std::make_tuple(self[pos].n_meth, self[pos].n_unmeth);
    },
    R"doc(
    Access the tuple (n_meth, n_unmeth) of numbers of methylated and
    unmethylated reads for the query interval corresponding to the given
    position in the container. These are returned by copy, so access times
    might differ for the get_n_meth and get_n_unmeth methods.

    Parameters
    ----------

    pos (int): The index of the query interval for which to get the numbers of
        methylated and unmethylated reads.
    )doc",
    "pos"_a);
  cls.def(
    "get_n_meth",
    [](const xfr::level_container<xfr::level_element_t> &self,
       const std::size_t pos) -> std::uint32_t {
      if (pos >= std::size(self))
        throw std::out_of_range("Index out of range");
      return self[pos].n_meth;
    },
    R"doc(
    Access the number of methylated observations for the query interval
    corresponding to the given position.

    Parameters
    ----------

    pos (int): The index of the query interval for which to get the number of
        methylated reads.
    )doc",
    "pos"_a);
  cls.def(
    "get_n_unmeth",
    [](const xfr::level_container<xfr::level_element_t> &self,
       const std::size_t pos) -> std::uint32_t {
      if (pos >= std::size(self))
        throw std::out_of_range("Index out of range");
      return self[pos].n_unmeth;
    },
    R"doc(
    Access the number of UNmethylated observations for the query interval
    corresponding to the given position.

    Parameters
    ----------

    pos (int): The index of the query interval for which to get the number of
        UNmethylated reads.
    )doc",
    "pos"_a);
  cls.def(
    "get_wmean",
    [](const xfr::level_container<xfr::level_element_t> &self,
       const std::size_t pos) -> double {
      if (pos >= std::size(self))
        throw std::out_of_range("Index out of range");
      return self[pos].get_wmean();
    },
    R"doc(
    Get the weighted mean methylation level for the interval corresponding to
    the given position, which is the number of methylated observations divided
    by the total number of observations.

    Parameters
    ----------

    pos (int): The index of the query interval for which to get the weighted
        mean methylation level.
    )doc",
    "pos"_a);
  cls.def("all_wmeans", &xfr::level_container<xfr::level_element_t>::get_wmeans,
          R"doc(
    Apply the 'get_wmean' function to all elements of this Levels object,
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
  cls.def(
    "__str__",
    [](const xfr::level_container<xfr::level_element_t> &self) -> std::string {
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
  nb::class_<transferase::level_container<transferase::level_element_covered_t>>
    &cls) -> void {
  using namespace nanobind::literals;  // NOLINT
  namespace xfr = transferase;
  cls.def(nb::init<>());
  cls.def("__len__",
          [](const xfr::level_container<xfr::level_element_covered_t> &self) {
            return std::size(self);
          });
  cls.def(
    "__getitem__",
    [](const xfr::level_container<xfr::level_element_covered_t> &self,
       const std::size_t pos)
      -> std::tuple<std::uint32_t, std::uint32_t, std::uint32_t> {
      if (pos >= std::size(self))
        throw std::out_of_range("Index out of range");
      return std::make_tuple(self[pos].n_meth, self[pos].n_unmeth,
                             self[pos].n_covered);
    },
    R"doc(
    Access the tuple (n_meth, n_unmeth, n_covered) of numbers of methylated
    and unmethylated reads, along with number of sites with at least one read,
    for the interval corresponding to the given position in the container.
    These are returned by copy, so access times might differ for the
    get_n_meth and get_n_unmeth methods.

    Parameters
    ----------

    pos (int): The index of the query interval for which to get the numbers of
        methylated and unmethylated reads, and covered sites.
    )doc",
    "pos"_a);
  cls.def(
    "get_n_meth",
    [](const xfr::level_container<xfr::level_element_covered_t> &self,
       const std::size_t pos) -> std::uint32_t {
      if (pos >= std::size(self))
        throw std::out_of_range("Index out of range");
      return self[pos].n_meth;
    },
    R"doc(
    Access the number of methylated observations for the query interval
    corresponding to the given position.

    Parameters
    ----------

    pos (int): The index of the interval for which to get the number
        of methylated reads.
    )doc",
    "pos"_a);
  cls.def(
    "get_n_unmeth",
    [](const xfr::level_container<xfr::level_element_covered_t> &self,
       const std::size_t pos) -> std::uint32_t {
      if (pos >= std::size(self))
        throw std::out_of_range("Index out of range");
      return self[pos].n_unmeth;
    },
    R"doc(
    Access the number of UNmethylated observations for the query interval
    corresponding to the given position.

    Parameters
    ----------

    pos (int): The index of the query interval for which to get the number of
        UNmethylated reads.
    )doc",
    "pos"_a);
  cls.def(
    "get_n_covered",
    [](const xfr::level_container<xfr::level_element_covered_t> &self,
       const std::size_t pos) -> std::uint32_t {
      if (pos >= std::size(self))
        throw std::out_of_range("Index out of range");
      return self[pos].n_covered;
    },
    R"doc(
    Access the number of covered sites in the interval corresponding
    to the given position.

    Parameters
    ----------

    pos (int): The index of the interval for which to get the number
        of covered sites.
    )doc",
    "pos"_a);
  cls.def(
    "get_wmean",
    [](const xfr::level_container<xfr::level_element_covered_t> &self,
       const std::size_t pos) -> double {
      if (pos >= std::size(self))
        throw std::out_of_range("Index out of range");
      return self[pos].get_wmean();
    },
    R"doc(
    Get the weighted mean methylation level for the interval corresponding to
    the given position, which is the number of methylated observations divided
    by the total number of observations.

    Parameters
    ----------

    pos (int): The index of the query interval for which to get the weighted
        mean methylation level.
    )doc",
    "pos"_a);
  cls.def("all_means",
          &xfr::level_container<xfr::level_element_covered_t>::get_wmeans,
          R"doc(
    Apply the 'get_wmean' function to all elements of this LevelsCovered
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
         [](const xfr::level_container<xfr::level_element_covered_t> &self)
           -> std::string {
           return std::format("MLevelsCovered size={}", std::size(self));
         })
    .doc() = R"doc(
    A MLevelsCovered represents methylation levels in each
    among a list of GenomicInterval objects. This is the object type
    that is returned from a transferase query if you request
    information about sites covered.
    )doc"
    //
    ;
}
