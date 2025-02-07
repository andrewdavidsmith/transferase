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
#include "client_config_python.hpp"
#include "query_container.hpp"

#include <client_config.hpp>
#include <level_element.hpp>
#include <methylome_client.hpp>

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
methylome_client_bindings(nb::class_<transferase::methylome_client> &cls)
  -> void {
  using namespace nanobind::literals;  // NOLINT
  namespace xfr = transferase;         // NOLINT
  cls
    .def_static(
      "init",
      []() {
        try {
          return xfr::methylome_client::initialize();
        }
        catch (std::runtime_error &e) {
          throw std::runtime_error(
            std::format("{} [Check that transferase is configured]", e.what()));
        }
      },
      R"doc(

    Get a MethylomeClient initialized with settings already configured
    by the current user.

    )doc")
    .def(
      "save_config",
      [](const xfr::methylome_client &self, const std::string &config_dir) {
        if (config_dir.empty())
          self.write();
        else
          self.write(config_dir);
      },
      R"doc(

    Saves your current configuration, overwriting any existing values
    that have already been saved.

    )doc",
      "config_dir"_a = std::string())
    .def_static(
      "reset_to_default_config",
      []() {
        const auto sys_conf_dir = xfr::find_system_config_dir();
        std::error_code error;
        xfr::methylome_client::reset_to_default_configuration_system_config(
          sys_conf_dir, error);
        if (error)
          throw std::system_error(error);
      },
      R"doc(

    Resets the user configuration to default values. This will erase
    any configuration changes you uhave made since first configuring
    transferase.

    )doc")
    .def_static(
      "config",
      [](const std::vector<std::string> &genomes,
         const xfr::download_policy_t download_policy) {
        const auto config = xfr::client_config_python::read_python();
        config.configure_python_system_config(genomes, download_policy);
      },
      R"doc(

    Does the work of configuring the client, accepting a list of
    genomes and an indicator to force redownloading. If both arguments
    are empty, the configuration will be written but no genome indexes
    will be downloaded. If you specify genomes, or request a download,
    this command will take roughly 15-30s per genome, depending on
    internet speed.

    Parameters
    ----------

    genomes (list[str]): A list of genomes, for example:
        ["hg38", "mm39", "bosTau9"]

    download_policy (DownloadPolicy): Indication of what to
        (re)download.

    )doc",
      "genomes"_a = std::vector<std::string>(),
      "download_policy"_a = xfr::download_policy_t::missing)
    .def("__repr__", &xfr::methylome_client::tostring)
    .def("available_genomes",
         nb::overload_cast<>(&xfr::methylome_client::available_genomes,
                             nb::const_))
    .def("configured_genomes",
         nb::overload_cast<>(&xfr::methylome_client::configured_genomes,
                             nb::const_))
    .def(
      "get_levels",
      nb::overload_cast<const std::vector<std::string> &,
                        const xfr::query_container &>(
        &xfr::methylome_client::get_levels<xfr::level_element_t>, nb::const_),
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
      "methylome_names"_a, "query"_a)
    .def(
      "get_levels",
      nb::overload_cast<const std::vector<std::string> &, const std::uint32_t>(
        &xfr::methylome_client::get_levels<xfr::level_element_t>, nb::const_),
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
      "methylome_names"_a, "bin_size"_a)
    .def("get_levels_covered",
         nb::overload_cast<const std::vector<std::string> &,
                           const xfr::query_container &>(
           &xfr::methylome_client::get_levels<xfr::level_element_covered_t>,
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
         "methylome_names"_a, "query"_a)
    .def(
      "get_levels_covered",
      nb::overload_cast<const std::vector<std::string> &, const std::uint32_t>(
        &xfr::methylome_client::get_levels<xfr::level_element_covered_t>,
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
      "methylome_names"_a, "bin_size"_a)
    .def_rw("hostname", &xfr::methylome_client::hostname,
            R"doc(

    URL or IP address for the remote transferase server.  Like
    transferase.usc.edu. This must be a valid hostname. Don't specify
    a protocol or slashes, just the hostname.  You should only change
    this if there is a problem setting the server or if you have setup
    your own server.

    )doc")
    .def_rw("port", &xfr::methylome_client::port,
            R"doc(

    The server port number. You will find this along with the hostname of
    the transferase server. If it has been setup using ClientConfig, then
    you don't have to worry about it.

    )doc")
    .def_rw("index_dir", &xfr::methylome_client::index_dir,
            R"doc(

    The directory where genome index files are stored. For human and
    mouse, this occupies roughly 200MB and for all available genomes
    the total size is under 3GB. This defaults to
    '${HOME}/.config/transferase/indexes' and there is no reason to
    change it unless you are working with your own methylomes and
    started the data analysis with your own reference genome.

    )doc")
    .def_rw("metadata_file", &xfr::methylome_client::metadata_file,
            R"doc(

    This file contains information about available methylomes,
    reference genomes, and biological sample information for available
    methylomes. By default this file is pulled from a remote server
    and can be updated.  As with 'index_dir' there is no reason to
    change this unless you are working with your own data.

    )doc")
    .doc() = R"doc(

    A MethylomeClient is an interface for querying a remote
    transferase server. Using the MethylomeClient to make queries
    ensures that the client and server are always communicating about
    the exact same reference genome, and not one that differs, for
    example, by inclusion of unassembled fragments or alternate
    haplotypes. It needs the hostname and port for the server, along
    with a directory for genome indexes and a MethBase2 metadata
    file. It is possible for queries to work without the 'index_dir'
    or 'metadata_file', but some functions might not work. It is
    possible to change the values of all instance variables.  But this
    information should be setup through the static interface of the
    MethylomeClient class, which provides configuration functions. So
    afterwards you likely won't have to specify any of these values.

    )doc"
    //
    ;
}
