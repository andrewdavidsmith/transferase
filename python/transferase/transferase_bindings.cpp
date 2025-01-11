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
bindings files. It may be incomplete, incorrect or include features that
are considered implementation detail and may vary between Python implementations.
When in doubt, consult the module reference at the location listed above.
)";

#include "genome_index_bindings.hpp"
#include "genomic_interval_bindings.hpp"
#include "level_container_bindings.hpp"
#include "methylome_bindings.hpp"
#include "methylome_resource_bindings.hpp"
#include "query_container_bindings.hpp"

#include <genome_index.hpp>
#include <genomic_interval.hpp>
#include <level_container.hpp>
#include <logger.hpp>
#include <methylome.hpp>
#include <methylome_resource.hpp>
#include <query_container.hpp>
#include <request.hpp>

#include <moduleobject.h>
#include <pybind11/options.h>
#include <pybind11/pybind11.h>

#include <cstdint>
#include <string>

namespace transferase {
enum class request_type_code : std::uint8_t;
}

namespace py = pybind11;

auto
initialize_transferase() -> void {
  transferase::logger::instance(transferase::shared_from_cout(), "Transferase",
                                transferase::log_level_t::debug);
}

PYBIND11_MODULE(transferase, the_module) {

  initialize_transferase();

  the_module.doc() = warning_message;

  auto GenomicInterval = py::class_<transferase::genomic_interval>(
    the_module, "GenomicInterval",
    "Representation of a genomic interval as chrom, start, stop (zero-based, "
    "half-open)");

  auto GenomeIndex = py::class_<transferase::genome_index>(
    the_module, "GenomeIndex", "An index of CpG sites in a genome");

  auto Methylome = py::class_<transferase::methylome>(
    the_module, "Methylome", "Representation of a methylome");

  auto QueryContainer = py::class_<transferase::query_container>(
    the_module, "QueryContainer", "A container for a methylome query");

  auto LevelContainer =
    py::class_<transferase::level_container<transferase::level_element_t>>(
      the_module, "LevelContainer", "A container for methylation levels");

  auto LevelContainerCovered = py::class_<
    transferase::level_container<transferase::level_element_covered_t>>(
    the_module, "LevelContainerCovered",
    "A container for methylation levels with information about covered sites");

  auto LocalMethylomeResource =
    py::class_<transferase::local_methylome_resource>(
      the_module, "LocalMethylomeResource",
      "Interface for locally available methylomes");

  auto RemoteMethylomeResource =
    py::class_<transferase::remote_methylome_resource>(
      the_module, "RemoteMethylomeResource",
      "An interface for remotely available methylomes");

  genomic_interval_bindings(GenomicInterval);
  genome_index_bindings(GenomeIndex);
  methylome_bindings(Methylome);
  query_container_bindings(QueryContainer);

  level_container_bindings(LevelContainer);
  level_container_covered_bindings(LevelContainerCovered);

  local_methylome_resource_bindings(LocalMethylomeResource);
  remote_methylome_resource_bindings(RemoteMethylomeResource);
}
