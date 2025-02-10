# MIT License
#
# Copyright (c) 2025 Andrew Smith
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

# The build types that pre-exist
set(CMAKE_BUILD_TYPES "Build;Debug;Release;RelWithDebInfo;MinSizeRel")
# The custom build types defined below
list(APPEND CMAKE_BUILD_TYPES
  "Distribute;DistributePython;UnitTests;PythonTests;StaticAnalysis"
)

if(NOT ${CMAKE_BUILD_TYPE} IN_LIST CMAKE_BUILD_TYPES)
  message(FATAL_ERROR "Build type not found: ${CMAKE_BUILD_TYPE}")
endif()

# Distribute
if(CMAKE_BUILD_TYPE STREQUAL "Distribute")
  message(STATUS "Using the Distribute build type")

  # Check compatibility
  if(BUILD_PYTHON OR
      UNIT_TESTS OR
      STATIC_ANALYSIS OR
      ENABLE_CODE_COVERAGE OR
      ENABLE_PYTHON_UNIT_TESTS OR
      ENABLE_SANITIZING)
    message(FATAL_ERROR "Specified options are incompatible with 'Distribute'")
  endif()

  # Inherit compile flags from 'Release'
  separate_arguments(FLAGS NATIVE_COMMAND ${CMAKE_CXX_FLAGS_RELEASE})
  list(APPEND GLOBAL_COMPILE_OPTIONS ${FLAGS})
  separate_arguments(FLAGS NATIVE_COMMAND ${CMAKE_EXE_LINKER_FLAGS_RELEASE})
  list(APPEND GLOBAL_LINKER_OPTIONS ${FLAGS})

  # Options
  set(USE_STATIC_LIBS on)
  set(ENABLE_LTO on)
  set(CONFIGURE_FOR_DISTRIBUTION on)
  set(STRIP_PATHS_FROM_BINARIES on)
  set(STRIP_SUB_LIST "")
endif()

# DistributePython
if(CMAKE_BUILD_TYPE STREQUAL "DistributePython")
  message(STATUS "Using the DistributePython build type")

  # Check compatibility
  if(UNIT_TESTS OR
      STATIC_ANALYSIS OR
      ENABLE_CODE_COVERAGE OR
      ENABLE_PYTHON_UNIT_TESTS OR
      ENABLE_SANITIZING)
    message(FATAL_ERROR
      "Specified options are incompatible with 'DistributePython'")
  endif()

  # Inherit compile flags from 'Release'
  separate_arguments(FLAGS NATIVE_COMMAND ${CMAKE_CXX_FLAGS_RELEASE})
  list(APPEND GLOBAL_COMPILE_OPTIONS ${FLAGS})
  separate_arguments(FLAGS NATIVE_COMMAND ${CMAKE_EXE_LINKER_FLAGS_RELEASE})
  list(APPEND GLOBAL_LINKER_OPTIONS ${FLAGS})

  # Options
  set(BUILD_PYTHON on)
  set(PACKAGE_PYTHON on)
  set(ENABLE_LTO on)
  set(USE_STATIC_LIBS on)
  set(CONFIGURE_FOR_DISTRIBUTION off)
  set(STRIP_PATHS_FROM_BINARIES on)
  set(STRIP_SUB_LIST "")
endif()

# UnitTests
if(CMAKE_BUILD_TYPE STREQUAL "UnitTests")
  message(STATUS "Using the UnitTests build type")

  # Options on
  set(ENABLE_UNIT_TESTS on)

  # Options off
  set(CONFIGURE_FOR_DISTRIBUTION off)
  set(STRIP_PATHS_FROM_BINARIES off)
endif()

# PythonTests
if(CMAKE_BUILD_TYPE STREQUAL "PythonTests")
  message(STATUS "Using the 'PythonTests' build type")

  # Options on
  set(BUILD_PYTHON on)
  set(ENABLE_PYTHON_UNIT_TESTS on)

  # Options off
  set(CONFIGURE_FOR_DISTRIBUTION off)
  set(STRIP_PATHS_FROM_BINARIES off)
endif()

# StaticAnalysis
if(CMAKE_BUILD_TYPE STREQUAL "StaticAnalysis")
  message(STATUS "Using the 'StaticAnalysis' build type")

  # Options on
  set(BUILD_PYTHON off)  ## Until we can wrangle nanobind
  set(ENABLE_UNIT_TESTS on)
  set(ENABLE_STATIC_ANALYSIS on)

  # Options off
  set(CONFIGURE_FOR_DISTRIBUTION off)
  set(STRIP_PATHS_FROM_BINARIES off)
  set(STRIP_SUB_LIST "")
endif()
