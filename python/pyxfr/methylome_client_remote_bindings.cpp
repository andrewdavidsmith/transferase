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

#include "methylome_client_remote_bindings.hpp"

#include <methylome_client_remote.hpp>

#include <client_config.hpp>
#include <level_container_md.hpp>
#include <level_element.hpp>
#include <methylome_client_base.hpp>
#include <query_container.hpp>

#include <listobject.h>  // for PyList_New??
#include <nanobind/nanobind.h>
// ADS: without these, the build succeeds, but runtime failure
#include <nanobind/stl/string.h>  // IWYU pragma: keep
#include <nanobind/stl/vector.h>  // IWYU pragma: keep

#include <cstdint>
#include <format>  // for std::vector??
#include <new>     // for operator new??
#include <string>
#include <type_traits>
#include <utility>  // for std::declval
#include <vector>

namespace transferase {
struct genomic_interval;
}

namespace nb = nanobind;

auto
methylome_client_remote_bindings(
  nb::class_<transferase::methylome_client_remote> &cls) -> void {
  using namespace nanobind::literals;  // NOLINT
  namespace xfr = transferase;         // NOLINT
  cls.def(nb::init<const std::string &>(),
          R"doc(
    Instantiate a MClient object with settings already configured by the
    current user.
    )doc",
          "config_dir"_a = std::string{});
  cls.def_rw("config", &xfr::methylome_client_remote::config,
             R"doc(
    The MConfig object associated with this MClient.
    )doc");
  cls.def("__repr__", &xfr::methylome_client_remote::tostring);
  cls.def(
    "get_index_dir",
    [](const xfr::methylome_client_remote &self) -> std::string {
      return self.config.get_index_dir();
    },
    R"doc(
    Get the index directory for this MClient.
    )doc");
  cls.def("configured_genomes",
          nb::overload_cast<>(&xfr::methylome_client_remote::configured_genomes,
                              nb::const_),
          R"doc(
    Get a list of the genomes that are already configured for this MClient.
    )doc");
  cls.def("get_levels",
          nb::overload_cast<const std::vector<std::string> &,
                            const xfr::query_container &>(
            &xfr::methylome_client_remote::get_levels<xfr::level_element_t>,
            nb::const_),
          R"doc(
    Query the server for methylation levels in each query interval and for
    each methylome in the list. For repeated queries using the same set of
    intervals, using a MQuery is the most efficient.

    Parameters
    ----------

    methylomes (list[str]): A list of methylome names. These must be the names
        of methylomes that exist on the server. These will usually be SRA
        accession numbers, and the server will immediately reject any names
        that include letters other than [a-zA-Z0-9_].  Queries involving too
        many methylomes will be rejected; this number is roughly 45.

    query (MQuery): A MQuery object constructed from a list of GenomicInterval
        objects using a GenomeIndex. These must be valid for the genome
        associated with the given methylome names.
    )doc",
          "methylomes"_a, "query"_a);
  cls.def("get_levels",
          nb::overload_cast<const std::vector<std::string> &,
                            const std::vector<xfr::genomic_interval> &>(
            &xfr::methylome_client_remote::get_levels<xfr::level_element_t>,
            nb::const_),
          R"doc(
    Query the server for methylation levels in each given genomic interval and
    for each methylome in the list.  This function internally constructs a
    MQuery.

    Parameters
    ----------

    methylomes (list[str]): A list of methylome names. These must be the names
        of methylomes that exist on the server. These will usually be SRA
        accession numbers, and the server will immediately reject any names
        that include letters other than [a-zA-Z0-9_].  Queries involving too
        many methylomes will be rejected; this number is roughly 45.

    intervals (list[GenomicInterval]): A list of GenomicInterval objects from
        the same reference genome as the methylomes in methylomes.
    )doc",
          "methylomes"_a, "intervals"_a);
  cls.def(
    "get_levels",
    nb::overload_cast<const std::vector<std::string> &, const std::uint32_t>(
      &xfr::methylome_client_remote::get_levels<xfr::level_element_t>,
      nb::const_),
    R"doc(
    Query the server for methylation levels in each non-overlapping genomic
    interval of the given size and for each specified methylome.

    Parameters
    ----------

    methylomes (list[str]): A list of methylome names. These must be the names
        of methylomes that exist on the server. These will usually be SRA
        accession numbers, and the server will immediately reject any names
        that include letters other than [a-zA-Z0-9_].  Queries involving too
        many methylomes will be rejected; this number is roughly 45.

    bin_size (int): A values specifying the size of non-overlapping intervals
        to request levels for. There is a minimum size, likely between 100 and
        200 to prevent server overload.
    )doc",
    "methylomes"_a, "bin_size"_a);
  cls.def(
    "get_levels_covered",
    nb::overload_cast<const std::vector<std::string> &,
                      const xfr::query_container &>(
      &xfr::methylome_client_remote::get_levels<xfr::level_element_covered_t>,
      nb::const_),
    R"doc(
    Query the server for methylation levels in each query interval and for
    each methylome in the list. For repeated queries using the same set of
    intervals, using a MQuery is the most efficient.

    Parameters
    ----------

    methylomes (list[str]): A list of methylome names. These must be the names
        of methylomes that exist on the server. These will usually be SRA
        accession numbers, and the server will immediately reject any names
        that include letters other than [a-zA-Z0-9_].  Queries involving too
        many methylomes will be rejected; this number is roughly 45.

    query (MQuery): A MQuery object constructed from a list of GenomicInterval
        objects using a GenomeIndex. These must be valid for the genome
        associated with the given methylome names.
    )doc",
    "methylomes"_a, "query"_a);
  cls.def(
    "get_levels_covered",
    nb::overload_cast<const std::vector<std::string> &,
                      const std::vector<xfr::genomic_interval> &>(
      &xfr::methylome_client_remote::get_levels<xfr::level_element_covered_t>,
      nb::const_),
    R"doc(
    Query the server for methylation levels in each given genomic interval and
    for each methylome in the list. Additionally returns information about the
    number of sites covered by reads in each interval. This function
    internally constructs a MQuery.

    Parameters
    ----------

    methylomes (list[str]): A list of methylome names. These must be the names
        of methylomes that exist on the server. These will usually be SRA
        accession numbers, and the server will immediately reject any names
        that include letters other than [a-zA-Z0-9_].  Queries involving too
        many methylomes will be rejected; this number is roughly 45.

    intervals (list[GenomicInterval]): A list of GenomicInterval objects from
        the same reference genome as the methylomes in methylomes.
    )doc",
    "methylomes"_a, "intervals"_a);
  cls.def(
    "get_levels_covered",
    nb::overload_cast<const std::vector<std::string> &, const std::uint32_t>(
      &xfr::methylome_client_remote::get_levels<xfr::level_element_covered_t>,
      nb::const_),
    R"doc(
    Query the server for methylation levels in each non-overlapping genomic
    interval of the given size and for each specified methylome.  Additionally
    returns information about the number of sites covered by reads in each
    interval.

    Parameters
    ----------

    methylomes (list[str]): A list of methylome names. These must be the names
        of methylomes that exist on the server. These will usually be SRA
        accession numbers, and the server will immediately reject any names
        that include letters other than [a-zA-Z0-9_].  Queries involving too
        many methylomes will be rejected; this number is roughly 45.

    bin_size (int): A values specifying the size of non-overlapping intervals
        to request levels for. There is a minimum size, likely between 100 and
        200 to prevent server overload.
    )doc",
    "methylomes"_a, "bin_size"_a);
  cls.doc() = R"doc(
    An MClient object is an interface for querying a remote transferase
    server. Using the MClient to make queries ensures that the client and
    server are always communicating about the exact same reference genome, and
    not one that differs, for example, by inclusion of unassembled fragments
    or alternate haplotypes. If you have not already setup transferase using
    the MConfig class (or with command line tools), it is possible to use a
    MClient, but the process is more complicated.
  )doc"
    //
    ;
}
