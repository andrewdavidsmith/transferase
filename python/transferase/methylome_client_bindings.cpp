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

#include "methylome_client_bindings.hpp"

#include "bindings_utils.hpp"
#include "query_container.hpp"

#include <client_config.hpp>
#include <level_element.hpp>
#include <methylome_client_remote.hpp>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace nb = nanobind;

auto
methylome_client_bindings(nb::class_<transferase::methylome_client_remote> &cls)
  -> void {
  using namespace nanobind::literals;  // NOLINT
  namespace xfr = transferase;         // NOLINT
  cls.def_static("get_client", &xfr::methylome_client_remote::get_client,
                 R"doc(

    Get a MethylomeClient initialized with settings already configured
    by the current user.

    )doc",
                 "config_dir"_a = std::string{});
  cls.def_rw("config", &xfr::methylome_client_remote::config,
             R"doc(

    The ClientConfig object associated with this MethylomeClient.

    )doc");
  cls.def("__repr__", &xfr::methylome_client_remote::tostring);
  cls.def("available_genomes",
          nb::overload_cast<>(&xfr::methylome_client_remote::available_genomes,
                              nb::const_));
  cls.def("configured_genomes",
          nb::overload_cast<>(&xfr::methylome_client_remote::configured_genomes,
                              nb::const_));
  cls.def("get_levels",
          nb::overload_cast<const std::vector<std::string> &,
                            const xfr::query_container &>(
            &xfr::methylome_client_remote::get_levels<xfr::level_element_t>,
            nb::const_),
          R"doc(

    Make a query for methylation levels in each of a given set of
    intervals, specified depending on query type.

    Parameters
    ----------

    methylome_names (list[str]): A list of methylome names. These must
        be the names of methylomes that exist on the server. These
        will usually be SRA accession numbers, and the server will
        immediately reject any names that include letters other than
        [a-zA-Z0-9_].  Queries involving too many methylomes will be
        rejected; this number is roughly 45.

    query (QueryContainer): A QueryContainer object constructed from a
        list of GenomicInterval objects using a GenomeIndex. These
        must be valid for the genome associated with the given
        methylome names.

    )doc",
          "methylome_names"_a, "query"_a);
  cls.def(
    "get_levels",
    nb::overload_cast<const std::vector<std::string> &, const std::uint32_t>(
      &xfr::methylome_client_remote::get_levels<xfr::level_element_t>,
      nb::const_),
    R"doc(

    Make a query for methylation levels in each non-overlapping
    genomic interval of the given size.

    Parameters
    ----------

    methylome_names (list[str]): A list of methylome names. These must
        be the names of methylomes that exist on the server. These
        will usually be SRA accession numbers, and the server will
        immediately reject any names that include letters other than
        [a-zA-Z0-9_].  Queries involving too many methylomes will be
        rejected; this number is roughly 45.

    bin_size (int): A values specifying the size of non-overlapping
        intervals to request levels for. There is a minimum size,
        likely between 100 and 200 to prevent server overload.

    )doc",
    "methylome_names"_a, "bin_size"_a);
  cls.def(
    "get_levels_covered",
    nb::overload_cast<const std::vector<std::string> &,
                      const xfr::query_container &>(
      &xfr::methylome_client_remote::get_levels<xfr::level_element_covered_t>,
      nb::const_),
    R"doc(

    Make a query for methylation levels and number of sites with reads
    in each of a given set of intervals, specified depending on query
    type.

    Parameters
    ----------

    methylome_names (list[str]): A list of methylome names. These must
        be the names of methylomes that exist on the server. These
        will usually be SRA accession numbers, and the server will
        immediately reject any names that include letters other than
        [a-zA-Z0-9_].  Queries involving too many methylomes will be
        rejected; this number is roughly 45.

    query (QueryContainer): A QueryContainer object constructed from a
        list of GenomicInterval objects using a GenomeIndex. These
        must be valid for the genome associated with the given
        methylome names.

    )doc",
    "methylome_names"_a, "query"_a);
  cls.def(
    "get_levels_covered",
    nb::overload_cast<const std::vector<std::string> &, const std::uint32_t>(
      &xfr::methylome_client_remote::get_levels<xfr::level_element_covered_t>,
      nb::const_),
    R"doc(

    Make a query for methylation levels, along with information about
    the number of sites covered by reads, in each non-overlapping
    genomic interval of the given size.

    Parameters
    ----------

    methylome_names (list[str]): A list of methylome names. These must
        be the names of methylomes that exist on the server. These
        will usually be SRA accession numbers, and the server will
        immediately reject any names that include letters other than
        [a-zA-Z0-9_].  Queries involving too many methylomes will be
        rejected; this number is roughly 45.

    bin_size (int): A values specifying the size of non-overlapping
        intervals to request levels for. There is a minimum size,
        likely between 100 and 200 to prevent server overload.

    )doc",
    "methylome_names"_a, "bin_size"_a);
  cls.doc() = R"doc(

    A MethylomeClient is an interface for querying a remote
    transferase server. Using the MethylomeClient to make queries
    ensures that the client and server are always communicating about
    the exact same reference genome, and not one that differs, for
    example, by inclusion of unassembled fragments or alternate
    haplotypes. If you have not already setup transferase using
    the ClientConfig class (or with command line tools), then instances
    of this class might be very difficult to use.

    )doc"
    //
    ;
}
