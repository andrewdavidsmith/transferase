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

static constexpr auto warning_message = R"(
https://github.com/andrewdavidsmith/transferase

The following documentation is automatically generated from the Python
bindings files. It may be incomplete, incorrect or include features that are
considered implementation detail and may vary between Python implementations.
When in doubt, consult the module reference at the location listed above.
)";

#include "client_config_bindings.hpp"
#include "genome_index_bindings.hpp"
#include "genomic_interval_bindings.hpp"
#include "level_container_bindings.hpp"
#include "methylome_bindings.hpp"
#include "methylome_client_local_bindings.hpp"
#include "methylome_client_remote_bindings.hpp"
#include "query_container_bindings.hpp"

#include <client_config.hpp>  // IWYU pragma: keep
#include <download_policy.hpp>
#include <genome_index.hpp>
#include <genomic_interval.hpp>
#include <level_container_md.hpp>
#include <level_element.hpp>
#include <logger.hpp>
#include <methylome.hpp>
#include <methylome_client_local.hpp>
#include <methylome_client_remote.hpp>
#include <query_container.hpp>

#include <moduleobject.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>  // IWYU pragma: keep
#include <nanobind/stl/vector.h>  // IWYU pragma: keep

#include <string>
#include <type_traits>  // IWYU pragma: keep
#include <utility>      // IWYU pragma: keep

auto
initialize_transferase() -> void {
  namespace xfr = transferase;
  xfr::logger::instance(xfr::shared_from_cout(), "Transferase",
                        xfr::log_level_t::error);
}

NB_MODULE(transferase, the_module) {
  namespace nb = nanobind;
  namespace xfr = transferase;

  the_module.doc() = warning_message;

  initialize_transferase();

  auto LogLevel = nb::enum_<xfr::log_level_t>(the_module, "LogLevel")
                    .value("debug", xfr::log_level_t::debug)
                    .value("info", xfr::log_level_t::info)
                    .value("warning", xfr::log_level_t::warning)
                    .value("error", xfr::log_level_t::error)
                    .value("critical", xfr::log_level_t::critical);

  the_module.def("set_log_level", [](const xfr::log_level_t lvl) {
    xfr::logger::instance().set_level(lvl);
  });

  auto DLPolicy = nb::enum_<xfr::download_policy_t>(the_module, "DLPolicy")
                    .value("none", xfr::download_policy_t::none)
                    .value("all", xfr::download_policy_t::all)
                    .value("missing", xfr::download_policy_t::missing)
                    .value("update", xfr::download_policy_t::update);

  auto MConfig = nb::class_<xfr::client_config>(
    the_module, "MConfig", "Class to help configuring transferase");

  auto GenomicInterval = nb::class_<xfr::genomic_interval>(
    the_module, "GenomicInterval",
    "Representation of a genomic interval as chrom, start, stop (zero-based, "
    "half-open)");

  auto GenomeIndex = nb::class_<xfr::genome_index>(
    the_module, "GenomeIndex", "An index of CpG sites in a genome");

  auto Methylome = nb::class_<xfr::methylome>(the_module, "Methylome",
                                              "Representation of a methylome");

  auto MQuery = nb::class_<xfr::query_container>(
    the_module, "MQuery", "A container for a methylome query");

  auto MLevels = nb::class_<xfr::level_container_md<xfr::level_element_t>>(
    the_module, "MLevels", "A container for methylation levels");

  auto MLevelsCovered = nb::class_<
    xfr::level_container_md<xfr::level_element_covered_t>>(
    the_module, "MLevelsCovered",
    "A container for methylation levels with information about covered sites");

  auto MClient = nb::class_<xfr::methylome_client_remote>(
    the_module, "MClient", "Client to get data from a remote methylome server");

  auto MClientLocal = nb::class_<xfr::methylome_client_local>(
    the_module, "MClientLocal", "Client to get data stored locally");

  client_config_bindings(MConfig);

  genomic_interval_bindings(GenomicInterval);
  genome_index_bindings(GenomeIndex);
  methylome_bindings(Methylome);
  query_container_bindings(MQuery);

  level_container_bindings(MLevels);
  level_container_covered_bindings(MLevelsCovered);

  methylome_client_local_bindings(MClientLocal);
  methylome_client_remote_bindings(MClient);
}
