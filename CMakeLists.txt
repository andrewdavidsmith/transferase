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
  VERSION 0.2.0
  DESCRIPTION "The transferase system for retrieving methylomes"
  HOMEPAGE_URL https://github.com/andrewdavidsmith/transferase
  LANGUAGES CXX)

# Set language version used
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSIONS off)  # prevents std=gnu++23
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include(CheckIncludeFileCXX)
include(CheckFunctionExists)

## ADS: this was needed for something and I can't recall what...
# Ensure that -fPIC is used when compiling the static library
# set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
set(DATADIR "${CMAKE_INSTALL_DATADIR}")

include(cmake/config_options.cmake)

if(ENABLE_STATIC_ANALYSIS)
  include(cmake/static_analysis.cmake)
endif()

if(ENABLE_UNIT_TESTS)
  include(cmake/unit_tests.cmake)
  if(ENABLE_CODE_COVERAGE)
    include(cmake/code_coverage.cmake)
  endif()
endif()

if(NOT UNABLE_UNIT_TESTS)
  include(cmake/packaging.cmake)
endif()

if(USE_STATIC_LIBS)
  include(cmake/static_libs.cmake)
endif()

# Turn on LTO for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

include(cmake/third_party.cmake)

# Seems this needs to come after some third-party deps are found
if(ENABLE_SANITIZING)
  include(cmake/sanitizing.cmake)
endif()

# Configure this dir and JSON file for any build. They are always
# needed in the right place with the version in the filename
set(BUILD_DATA_DIR "${CMAKE_BINARY_DIR}/${DATADIR}/transferase")
file(MAKE_DIRECTORY ${BUILD_DATA_DIR})
configure_file(${CMAKE_SOURCE_DIR}/data/transferase_data.json
  ${BUILD_DATA_DIR}/transferase_data_${PROJECT_VERSION}.json
  COPYONLY
)

configure_file(config.h.in config.h)

add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wfatal-errors)

if(ENABLE_UNIT_TESTS)
  add_subdirectory(test)
endif()

add_subdirectory(src)

if(NOT ENABLE_UNIT_TESTS)
  include(cmake/install.cmake)
endif()
