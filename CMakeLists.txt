# MIT License
#
# Copyright (c) 2024 Andrew Smith
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# to find the version of cmake do
# $ cmake --version
cmake_minimum_required(VERSION 3.30)
project(
  transferase
  VERSION 0.3.0
  DESCRIPTION "The transferase system for retrieving methylomes"
  HOMEPAGE_URL https://github.com/andrewdavidsmith/transferase
  LANGUAGES CXX)

# Make sure our modules are found first
list(PREPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/modules)

# Set language version used
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSIONS off)  # prevents std=gnu++23
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include(CheckIncludeFileCXX)
include(CheckFunctionExists)
include(CheckCXXCompilerFlag)

include(GNUInstallDirs)

# Collect any global linker options as needed and then apply them
# individually to targets
set(GLOBAL_COMPILE_OPTIONS "")
set(GLOBAL_LINKER_OPTIONS "")

include(cmake/build_types.cmake)
include(cmake/config_options.cmake)
include(cmake/data_config.cmake)

if(USE_STATIC_LIBS)
  # This needs to come before finding any libraries so that the static
  # versions are identified
  include(cmake/static_libs.cmake)
endif()

if(ENABLE_STATIC_ANALYSIS)
  include(cmake/static_analysis.cmake)
endif()

include(cmake/third_party.cmake)

if(CONFIGURE_FOR_DISTRIBUTION)
  include(cmake/packaging.cmake)
endif()

if(BUILD_PYTHON)
  message(STATUS "Building with position-independent code for Python API")
  # Ensure that -fPIC is used when compiling the static library so
  # pybind11 works
  set(CMAKE_POSITION_INDEPENDENT_CODE on)
endif()

if(ENABLE_LTO)
  # Turn on LTO if we are building for distribution
  include(CheckIPOSupported)
  check_ipo_supported(RESULT result OUTPUT output)
  if(result)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(FATAL_ERROR "IPO is not supported: ${output}")
  endif()
endif()

if(ENABLE_UNIT_TESTS OR ENABLE_PYTHON_UNIT_TESTS)
  include(cmake/unit_tests.cmake)
  if(ENABLE_CODE_COVERAGE)
    include(cmake/code_coverage.cmake)
  endif()
endif()

# Seems this needs to come after some third-party deps are found
if(ENABLE_SANITIZING)
  include(cmake/sanitizing.cmake)
endif()

# ADS: set the most stringent warnings we can
list(APPEND GLOBAL_COMPILE_OPTIONS
  -Wall -Wextra -Wpedantic -Werror -Wfatal-errors
)

if(STRIP_PATHS_FROM_BINARIES)
  # This is set if we have configured to distribute
  set(PREFIX_MAP_ARG -ffile-prefix-map=)
  list(TRANSFORM STRIP_SUB_LIST PREPEND ${PREFIX_MAP_ARG})
  list(APPEND GLOBAL_COMPILE_OPTIONS ${STRIP_SUB_LIST})
endif()

message(STATUS "Finished global compile and linker configuration")

add_subdirectory(src)
if(ENABLE_UNIT_TESTS)
  add_subdirectory(test)
endif()
if(BUILD_PYTHON)
  add_subdirectory(python)
endif()

# Putting install at the end
include(cmake/install.cmake)
