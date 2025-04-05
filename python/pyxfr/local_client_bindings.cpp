/* MIT License
 *
 * Copyright (c) 2025 Andrew D Smith
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

#include "local_client_bindings.hpp"

#include <local_client.hpp>

#include <client_base.hpp>
#include <client_config.hpp>
#include <level_container_md.hpp>
#include <level_element.hpp>
#include <query_container.hpp>

#include <listobject.h>  // for PyList_New
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>  // IWYU pragma: keep
#include <nanobind/stl/vector.h>  // IWYU pragma: keep

#include <cstdint>
#include <format>
#include <new>  // for operator new
#include <string>
#include <type_traits>
#include <utility>  // for std::declval
#include <vector>

namespace transferase {
struct genomic_interval;
}  // namespace transferase

namespace nb = nanobind;

auto
local_client_bindings(nb::class_<transferase::local_client> &cls) -> void {
  using namespace nanobind::literals;  // NOLINT
  namespace xfr = transferase;         // NOLINT
  using xfr::local_client;

  cls.def(nb::init<const std::string &>(),
          R"doc(
    Instantiate a MClientLocal initialized with settings already configured,
    either in a specified directory or using pre-configured defaults.

    Parameters
    ----------

    config_dir (str): [Optional] Directory to look for configuration.  This is
        used to locate genome indexes and directories where methylomes are
        stored.
    )doc",
          "config_dir"_a = std::string{});

  cls.def_rw("config", &local_client::config,
             R"doc(
    The MConfig object associated with this MClientLocal. You can use this to
    examine directly examine the current configuration values.
    )doc");

  cls.def_rw("config", &local_client::config,
             R"doc(
    The MConfig object associated with this MClientLocal. You can use this to
    examine directly examine the current configuration values.
    )doc");

  cls.def("__repr__", &local_client::tostring);

  cls.def(
    "get_index_dir",
    [](const local_client &self) -> std::string {
      return self.config.get_index_dir();
    },
    R"doc(
    Get the index directory for this MClientLocal.
    )doc");

  cls.def(
    "get_methylome_dir",
    [](const local_client &self) -> std::string {
      return self.config.get_methylome_dir();
    },
    R"doc(
    Get the methylomes directory for this MClientLocal.
    )doc");

  cls.def("configured_genomes",
          nb::overload_cast<>(&local_client::configured_genomes, nb::const_),
          R"doc(
    Get a list of the genomes that are already configured for this
    MClientLocal.
    )doc");

  cls.def("get_levels",
          nb::overload_cast<const std::vector<std::string> &,
                            const xfr::query_container &>(
            &local_client::get_levels<xfr::level_element_t>, nb::const_),
          R"doc(
    Query a local directory for methylation levels in each of a given set of
    intervals and for each methylome in the list. For repeated queries using
    the same set of intervals, using a MQuery is the most efficient.

    Parameters
    ----------

    methylomes (list[str]): A list of methylome names. These must be the names
        of methylomes that exist in the methylome directory for this
        MClientLocal.

    query (MQuery): A MQuery object constructed from a list of GenomicInterval
        objects using a GenomeIndex. These must be valid for the genome
        associated with the given methylome names.
    )doc",
          "methylomes"_a, "query"_a);

  cls.def("get_levels",
          nb::overload_cast<const std::vector<std::string> &,
                            const std::vector<xfr::genomic_interval> &>(
            &local_client::get_levels<xfr::level_element_t>, nb::const_),
          R"doc(
    Query a local directory for methylation levels in each given genomic
    interval and for each methylome in the list.  This function internally
    constructs a MQuery.

    Parameters
    ----------

    methylomes (list[str]): A list of methylome names. These must be the names
        of methylomes that exist in the methylome directory for this
        MClientLocal.

    intervals (list[GenomicInterval]): A list of GenomicInterval objects from
        the same reference genome as the methylomes in methylomes.
    )doc",
          "methylomes"_a, "intervals"_a);

  cls.def(
    "get_levels",
    nb::overload_cast<const std::vector<std::string> &, const std::uint32_t>(
      &local_client::get_levels<xfr::level_element_t>, nb::const_),
    R"doc(
    Query a local directory for methylation levels in each non-overlapping
    genomic interval of the given size and for each specified methylome.

    Parameters
    ----------

    methylomes (list[str]): A list of methylome names. These must be the names
        of methylomes that exist in the methylome directory for this
        MClientLocal.

    bin_size (int): A values specifying the size of non-overlapping intervals
        to request levels for.
    )doc",
    "methylomes"_a, "bin_size"_a);

  cls.def(
    "get_levels_covered",
    nb::overload_cast<const std::vector<std::string> &,
                      const xfr::query_container &>(
      &local_client::get_levels<xfr::level_element_covered_t>, nb::const_),
    R"doc(
    Query a local directory for methylation levels in each of a given set of
    intervals and for each methylome in the list. For repeated queries using
    the same set of intervals, using a MQuery is the most efficient.
    Additionally returns information about the number of sites covered by
    reads in each interval.

    Parameters
    ----------

    methylomes (list[str]): A list of methylome names. These must be the names
        of methylomes that exist in the methylome directory for this
        MClientLocal.

    query (MQuery): A MQuery object constructed from a list of GenomicInterval
        objects using a GenomeIndex. These must be valid for the genome
        associated with the given methylome names.
    )doc",
    "methylomes"_a, "query"_a);

  cls.def(
    "get_levels_covered",
    nb::overload_cast<const std::vector<std::string> &,
                      const std::vector<xfr::genomic_interval> &>(
      &local_client::get_levels<xfr::level_element_covered_t>, nb::const_),
    R"doc(
    Query a local directory for methylation levels in each given genomic
    interval and for each methylome in the list.  This function internally
    constructs a MQuery.  Additionally returns information about the number of
    sites covered by reads in each interval.

    Parameters
    ----------

    methylomes (list[str]): A list of methylome names. These must be the names
        of methylomes that exist in the methylome directory for this
        MClientLocal.

    intervals (list[GenomicInterval]): A list of GenomicInterval objects from
        the same reference genome as the methylomes in methylomes.
    )doc",
    "methylomes"_a, "intervals"_a);

  cls.def(
    "get_levels_covered",
    nb::overload_cast<const std::vector<std::string> &, const std::uint32_t>(
      &local_client::get_levels<xfr::level_element_covered_t>, nb::const_),
    R"doc(
    Query a local directory for methylation levels in each non-overlapping
    genomic interval of the given size and for each specified methylome.
    Additionally returns information about the number of sites covered by
    reads in each interval.

    Parameters
    ----------

    methylomes (list[str]): A list of methylome names. These must be the names
        of methylomes that exist in the methylome directory for this
        MClientLocal.

    bin_size (int): A values specifying the size of non-overlapping intervals
        to request levels for. There is a minimum size, likely between 100 and
        200 to prevent server overload.
    )doc",
    "methylomes"_a, "bin_size"_a);

  cls.doc() = R"doc(
    A MClientLocal object is an interface for querying methylomes stored in
    local directory. Using an MClientLocal object to make queries ensures that
    the queries are always consistent with the exact same reference genome
    used to analyze each methylome. Before instantiating an MClientLocal
    object, you should configure transferase on your system either through the
    MConfig class or by using the transferase command line app.

    When you do a transferase query using an MClientLocal object, you must
    provide the name of the reference genome. This might often seem redundant,
    but it allows for several important consistency checks and makes sure
    analyses does not switch genomes by accident, or attempt to use query
    intervals from species that do not correspond to methylomes in the query.
    Think of it as an extra layer of safety. In theory this requirement can be
    eliminated through different implementation choices, but currently this is
    deemed the most reliable approach.
    )doc"
    //
    ;
}
