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

#include "methylome_bindings.hpp"

#include <genome_index.hpp>   // IWYU pragma: keep
#include <level_element.hpp>  // for level_element_covered_t (ptr only)
#include <methylome.hpp>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <format>  // for std::format
#include <string>
#include <system_error>  // for std::error_code, std::system_error
#include <tuple>
#include <variant>  // for std::tuple

namespace transferase {

struct query_container;
template <typename level_element_type> struct level_container_md;

}  // namespace transferase

namespace nb = nanobind;

auto
methylome_bindings(nb::class_<transferase::methylome> &cls) -> void {
  using namespace nanobind::literals;  // NOLINT
  cls.def(nb::init<>())
    .def_static("read",
                nb::overload_cast<const std::string &, const std::string &>(
                  &transferase::methylome::read),
                R"doc(
    Read a methylome object from the filesystem.

    Parameters
    ----------

    directory_name (str): Directory where the methylome is stored.

    methylome_name (str): Name of the methylome to read.
    )doc",
                "directory_name"_a, "methylome_name"_a)
    .def(
      "is_consistent",
      nb::overload_cast<>(&transferase::methylome::is_consistent, nb::const_),
      R"doc(
    Returns true if and only if a methylome is internally consistent.
    )doc")
    .def("is_consistent",
         nb::overload_cast<const transferase::methylome &>(
           &transferase::methylome::is_consistent, nb::const_),
         R"doc(
    Returns true iff two methylomes are consistent with each
    other. This means they are the same size, and are based on the
    same reference genome.

    Parameters
    ----------

    other (Methylome): The other methylome to check for consistency
        with self.
    )doc",
         "other"_a);
  cls
    .def("write",
         nb::overload_cast<const std::string &, const std::string &>(
           &transferase::methylome::write, nb::const_),
         R"doc(
    Write this methylome to a directory.

    Parameters
    ----------

    directory_name (str): The directory in where this methylome should
        be written.

    methylome_name (str): The name of the methylome; determines
        filenames written.
    )doc",
         "directory_name"_a, "methylome_name"_a)
    .def(
      "init_metadata",
      [](transferase::methylome &self,
         const transferase::genome_index &index) -> void {
        const auto error = self.init_metadata(index);
        if (error)
          throw std::system_error(error);
      },
      R"doc(
    Initialize the metadata associated with this methylome.
    This information is used while constructing a methylome and is
    based on the given GenomeIndex.

    Parameters
    ----------

    index (GenomeIndex): A GenomeIndex created from the exact same
        reference genome as was used to map the reads when producing
        this Methylome.
    )doc",
      "index"_a)
    .def("update_metadata",
         [](transferase::methylome &self) -> void {
           const auto error = self.update_metadata();
           if (error)
             throw std::system_error(error);
         })
    .def("add", &transferase::methylome::add, "other"_a)
    .def("__repr__", &transferase::methylome::tostring,
         R"doc(
    Generate a string representation of a methylome in JSON format.
      )doc")
    .def("get_levels",
         nb::overload_cast<const transferase::query_container &>(
           &transferase::methylome::get_levels<transferase::level_element_t>,
           nb::const_),
         "query"_a)
    .def(
      "get_levels",
      nb::overload_cast<const std::uint32_t, const transferase::genome_index &>(
        &transferase::methylome::get_levels<transferase::level_element_t>,
        nb::const_),
      "bin_size"_a, "genome_index"_a)
    .def("get_levels_covered",
         nb::overload_cast<const transferase::query_container &>(
           &transferase::methylome::get_levels<
             transferase::level_element_covered_t>,
           nb::const_),
         "query"_a)
    .def(
      "get_levels_covered",
      nb::overload_cast<const std::uint32_t, const transferase::genome_index &>(
        &transferase::methylome::get_levels<
          transferase::level_element_covered_t>,
        nb::const_),
      "bin_size"_a, "genome_index"_a)
    .def("global_levels",
         [](const transferase::methylome &self)
           -> std::tuple<std::uint32_t, std::uint32_t> {
           const auto result =
             self.global_levels<transferase::level_element_t>();
           return std::make_tuple(result.n_meth, result.n_unmeth);
         })
    .def("global_levels_covered",
         [](const transferase::methylome &self)
           -> std::tuple<std::uint32_t, std::uint32_t, std::uint32_t> {
           const auto result =
             self.global_levels<transferase::level_element_covered_t>();
           return std::make_tuple(result.n_meth, result.n_unmeth,
                                  result.n_covered);
         })
    //
    ;
}
