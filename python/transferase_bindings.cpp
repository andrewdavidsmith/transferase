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

#include <cpg_index.hpp>
#include <cpg_index_data.hpp>
#include <cpg_index_metadata.hpp>
#include <level_container.hpp>
#include <level_element.hpp>
#include <methylome.hpp>
#include <methylome_data.hpp>
#include <methylome_metadata.hpp>
#include <methylome_resource.hpp>
#include <query_container.hpp>
#include <request.hpp>

#include <genomic_interval.hpp>
#include <logger.hpp>

#include <cpg_index_bindings.hpp>
#include <cpg_index_data_bindings.hpp>
#include <cpg_index_metadata_bindings.hpp>
#include <error_code_bindings.hpp>
#include <genomic_interval_bindings.hpp>
#include <level_container_bindings.hpp>
#include <level_element_bindings.hpp>
#include <methylome_bindings.hpp>
#include <methylome_data_bindings.hpp>
#include <methylome_metadata_bindings.hpp>
#include <methylome_resource_bindings.hpp>
#include <query_container_bindings.hpp>
#include <request_bindings.hpp>

#include <moduleobject.h>
#include <pybind11/pybind11.h>

#include <cstdint>
#include <string>
#include <system_error>

namespace xfrase {
enum class request_type_code : std::uint8_t;
}

namespace py = pybind11;

auto
initialize_transferase() -> void {
  xfrase::logger::instance(xfrase::shared_from_cout(), "Transferase",
                           xfrase::log_level_t::debug);
  // Your C++ initialization code here, such as setting up resources or data
}

PYBIND11_MODULE(transferase, m) {
  initialize_transferase();

  m.doc() = "Python API for transferase";  // optional module docstring

  auto ErrorCode = py::class_<std::error_code>(
    m, "ErrorCode",
    "Error code object used by several functions in transferase");

  auto GenomicInterval = py::class_<xfrase::genomic_interval>(
    m, "GenomicInterval",
    "Representation of a genomic interval as chrom, start, stop (zero-based, "
    "half-open)");

  auto CpgIndexData = py::class_<xfrase::cpg_index_data>(
    m, "CpgIndexData", "Data part of a CpG index");

  auto CpgIndexMetadata = py::class_<xfrase::cpg_index_metadata>(
    m, "CpgIndexMetadata", "Metadata part of a CpG index");

  auto CpgIndex = py::class_<xfrase::cpg_index>(
    m, "CpgIndex", "An index of CpG sites in a genome");

  auto MethylomeMetadata = py::class_<xfrase::methylome_metadata>(
    m, "MethylomeMetadata", "Metadata part of a methylome");

  auto MethylomeData = py::class_<xfrase::methylome_data>(
    m, "MethylomeData", "Data part of a methylome");

  auto Methylome = py::class_<xfrase::methylome>(
    m, "Methylome", "Representation of a methylome");

  auto LevelElement = py::class_<xfrase::level_element_t>(
    m, "LevelElement", "Methylation level for a genomic interval");

  auto LevelElementCovered = py::class_<xfrase::level_element_covered_t>(
    m, "LevelElementCovered",
    "Methylation level for a genomic interval with number of sites covered");

  auto LevelContainer =
    py::class_<xfrase::level_container<xfrase::level_element_t>>(
      m, "LevelContainer", "A container for methylation levels");

  auto LevelContainerCovered =
    py::class_<xfrase::level_container<xfrase::level_element_covered_t>>(
      m, "LevelContainerCovered",
      "A container for methylation levels with information about covered "
      "sites");

  auto QueryContainer = py::class_<xfrase::query_container>(
    m, "QueryContainer", "A container for a methylome query");

  auto LocalMethylomeResource = py::class_<xfrase::local_methylome_resource>(
    m, "LocalMethylomeResource", "Interface for locally available methylomes");

  auto RemoteMethylomeResource = py::class_<xfrase::remote_methylome_resource>(
    m, "RemoteMethylomeResource",
    "An interface for remotely available methylomes");

  auto RequestTypeCode = py::enum_<xfrase::request_type_code>(
    m, "RequestTypeCode", "Codes for the various request types");

  auto Request = py::class_<xfrase::request>(m, "Request", "A request");

  error_code_bindings(ErrorCode);

  genomic_interval_bindings(GenomicInterval);

  cpg_index_metadata_bindings(CpgIndexMetadata);
  cpg_index_data_bindings(CpgIndexData);
  cpg_index_bindings(CpgIndex);

  methylome_metadata_bindings(MethylomeMetadata);
  methylome_data_bindings(MethylomeData);
  methylome_bindings(Methylome);

  level_element_bindings(LevelElement);
  level_element_covered_bindings(LevelElementCovered);
  level_container_bindings(LevelContainer);
  level_container_covered_bindings(LevelContainerCovered);
  query_container_bindings(QueryContainer);

  request_type_code_bindings(RequestTypeCode);
  request_bindings(Request);

  local_methylome_resource_bindings(LocalMethylomeResource);
  remote_methylome_resource_bindings(RemoteMethylomeResource);
}
