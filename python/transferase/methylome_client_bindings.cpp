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

#include <level_element.hpp>
#include <methylome_client.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>  // IWYU pragma: keep

#include <cstdint>
#include <string>
#include <vector>

namespace transferase {
struct query_container;
template <typename level_element_type> struct level_container;
}  // namespace transferase

namespace py = pybind11;

auto
methylome_client_bindings(py::class_<transferase::methylome_client> &cls)
  -> void {
  using namespace pybind11::literals;  // NOLINT
  cls
    .def(
      py::init<const std::string &, const std::string &, const std::uint64_t>(),
      "hostname"_a, "port_number"_a, "index_hash"_a)
    .def_static(
      "get_default",
      []() { return transferase::methylome_client::get_default(); },
      R"doc(

    Get a MethylomeClient initialized using the default client
    configuration file.

    )doc")
    .def("__repr__", &transferase::methylome_client::tostring)
    .def(
      "get_levels",
      [](const transferase::methylome_client &self,
         const std::vector<std::string> &methylome_names,
         const transferase::query_container &query) {
        return self.get_levels<transferase::level_element_t>(methylome_names,
                                                             query);
      },
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

    )doc")
    .def(
      "get_levels_covered",
      [](const transferase::methylome_client &self,
         const std::vector<std::string> &methylome_names,
         const transferase::query_container &query) {
        return self.get_levels<transferase::level_element_covered_t>(
          methylome_names, query);
      },
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

    )doc")
    .def_readwrite("hostname", &transferase::methylome_client::hostname,
                   R"doc(

    The name of the server. Like transferase.usc.edu. This must be a
    valid hostname. Don't specify a protocol or slashes, just the
    hostname. Note: you can also use the IP address.

    )doc")
    .def_readwrite("port_number", &transferase::methylome_client::port_number,
                   R"doc(

    The server port number. You will find this along with the hostname of
    the transferase server. If it has been setup using ClientConfig, then
    you don't have to worry about it.

    )doc")
    .def_readwrite("index_hash", &transferase::methylome_client::index_hash,
                   R"doc(

    A number that identifies the reference genome. This number is
    associated with a GenomeIndex object, and ensures that the client
    and server are talking about the exact same reference genome.  You
    don't need to provide this number with each query, but if you
    start making queries about a different species, then you will need
    to update this value.

    )doc")
    .doc() = R"doc(

    A MethylomeClient is an interface for querying a remote
    transferase server. It needs the hostname and port for the server,
    which can be set directly with the 'hostname' and 'port_number'
    instance variables.  But this information can also be setup
    through the ClientConfig interface and afterwards you won't have
    to specify it. Using the MethylomeClient to make queries requires
    setting an 'index_hash' value to ensure that the intervals you are
    using make sense for the methylomes you specify.  This ensure that
    you and the server are referring to the exact same reference
    genome, and not one that differs, for example, by inclusion of
    unassembled fragments or alternate haplotypes. See GenomeIndex for
    more information about the 'index_hash'.

    )doc"
    //
    ;
}
