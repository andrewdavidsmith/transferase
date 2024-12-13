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
  mxe
  VERSION 0.1.0
  DESCRIPTION "Methylome transfer engine"
  HOMEPAGE_URL https://github.com/andrewdavidsmith/mxe
  LANGUAGES CXX)

# Set language version used
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED yes)
set(CMAKE_CXX_EXTENSIONS off)  # prevents std=gnu++23
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

option(ENABLE_UNIT_TESTS "Enable unit tests" OFF)

if(MXE_STATIC_ANALYSIS)
  # cpplint: all options are in the config file
  set(CMAKE_CXX_CPPLINT cpplint --quiet)
  set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE
    include-what-you-use
    -Xiwyu
    --mapping_file=${PROJECT_SOURCE_DIR}/iwyu.imp
  )
  # cppcheck: options on the command line as there is no config file
  set(CMAKE_CXX_CPPCHECK
    cppcheck
    --quiet
    --enable=all
    --check-level=exhaustive
    --suppressions-list=${PROJECT_SOURCE_DIR}/.cppcheck_suppress
  )
  # clang-tidy: currently needs at least v20.0
  # set(CMAKE_CXX_CLANG_TIDY
  #   clang-tidy
  #   --quiet
  #   -p Build
  # )
endif()

include(CheckIncludeFileCXX)
include(CheckFunctionExists)

if(MXE_USE_STATIC_LIBS)
  set(Boost_USE_STATIC_LIBS on)
  set(ZLIB_USE_STATIC_LIBS on)
endif()

find_package(Threads REQUIRED)
find_package(
  Boost
  1.85.0 REQUIRED
  COMPONENTS
  program_options json)
find_package(ZLIB REQUIRED)

if(ENABLE_UNIT_TESTS)
  message(STATUS "Unit tests enabled")
  add_compile_definitions(UNIT_TEST)
  enable_testing()
  add_subdirectory(tests)
endif()

add_subdirectory(src)

configure_file(config.h.in config.h)

# for installation:
#
# $ cmake --install . --prefix /path/to/install/root

# Get ready for packaging
set(CPACK_PACKAGE_VENDOR "Andrew D Smith")
set(CPACK_GENERATOR "TGZ;STGZ")
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE)
include(CPack)
