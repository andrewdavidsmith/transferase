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
bindings files. It may be incomplete, incorrect or include features
that are considered implementation detail and may vary between Python
implementations.  When in doubt, consult the module reference at the
location listed above.
)";

#include "client_config_bindings.hpp"
#include "genome_index_bindings.hpp"
#include "genomic_interval_bindings.hpp"
#include "level_container_bindings.hpp"
#include "methylome_bindings.hpp"
#include "methylome_client_bindings.hpp"
#include "query_container_bindings.hpp"

#include <client_config.hpp>  // IWYU pragma: keep
#include <download_policy.hpp>
#include <genome_index.hpp>
#include <genomic_interval.hpp>
#include <level_container.hpp>
#include <level_element.hpp>
#include <logger.hpp>
#include <methylome.hpp>
#include <methylome_client_remote.hpp>
#include <methylome_directory.hpp>
#include <query_container.hpp>

#include <moduleobject.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

// ADS: the header below has functions to control what is
// auto-generated in the python docs
// #include <nanobind/options.h>  // IWYU pragma: keep

#include <string>
#include <type_traits>  // for std::is_lvalue_reference_v
#include <utility>      // for std::move

namespace nb = nanobind;

auto
initialize_transferase() -> void {
  transferase::logger::instance(transferase::shared_from_cout(), "Transferase",
                                transferase::log_level_t::error);
}

NB_MODULE(transferase, the_module) {
  // nb::options options;
  // options.disable_function_signatures();
  the_module.doc() = warning_message;

  initialize_transferase();

  auto LogLevel = nb::enum_<transferase::log_level_t>(the_module, "LogLevel")
                    .value("debug", transferase::log_level_t::debug)
                    .value("info", transferase::log_level_t::info)
                    .value("warning", transferase::log_level_t::warning)
                    .value("error", transferase::log_level_t::error)
                    .value("critical", transferase::log_level_t::critical);

  the_module.def("set_log_level", [](const transferase::log_level_t lvl) {
    transferase::logger::instance().set_level(lvl);
  });

  auto DownloadPolicy =
    nb::enum_<transferase::download_policy_t>(the_module, "DownloadPolicy")
      .value("none", transferase::download_policy_t::none)
      .value("all", transferase::download_policy_t::all)
      .value("missing", transferase::download_policy_t::missing)
      .value("update", transferase::download_policy_t::update);

  auto ClientConfig = nb::class_<transferase::client_config>(
    the_module, "ClientConfig", "Class to help configuring transferase");

  auto GenomicInterval = nb::class_<transferase::genomic_interval>(
    the_module, "GenomicInterval",
    "Representation of a genomic interval as chrom, start, stop (zero-based, "
    "half-open)");

  auto GenomeIndex = nb::class_<transferase::genome_index>(
    the_module, "GenomeIndex", "An index of CpG sites in a genome");

  auto Methylome = nb::class_<transferase::methylome>(
    the_module, "Methylome", "Representation of a methylome");

  auto QueryContainer = nb::class_<transferase::query_container>(
    the_module, "QueryContainer", "A container for a methylome query");

  auto LevelContainer =
    nb::class_<transferase::level_container<transferase::level_element_t>>(
      the_module, "LevelContainer", "A container for methylation levels");

  auto LevelContainerCovered = nb::class_<
    transferase::level_container<transferase::level_element_covered_t>>(
    the_module, "LevelContainerCovered",
    "A container for methylation levels with information about covered sites");

  auto MethylomeClient = nb::class_<transferase::methylome_client_remote>(
    the_module, "MethylomeClient",
    "Client to get data from a remote methylome server");

  client_config_bindings(ClientConfig);

  genomic_interval_bindings(GenomicInterval);
  genome_index_bindings(GenomeIndex);
  methylome_bindings(Methylome);
  query_container_bindings(QueryContainer);

  level_container_bindings(LevelContainer);
  level_container_covered_bindings(LevelContainerCovered);

  /// ADS: leaving out of v0.4.0 bindings
  // methylome_directory_bindings(MethylomeDirectory);
  methylome_client_bindings(MethylomeClient);
}
