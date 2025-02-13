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
  "ReleasePython;Test;TestPython;StaticAnalysis"
)

if(NOT ${CMAKE_BUILD_TYPE} IN_LIST CMAKE_BUILD_TYPES)
  string(REPLACE ";"  "\n - " BUILD_TYPES_FORMATTED "${CMAKE_BUILD_TYPES}")
  string(PREPEND BUILD_TYPES_FORMATTED "\n - ")
  message("Build type not found: ${CMAKE_BUILD_TYPE}\n"
    "Options are: ${BUILD_TYPES_FORMATTED}"
  )
  message(FATAL_ERROR "Terminating due to incorrect build type")
endif()

set(BUILD_CLI on)
if(BUILD_PYTHON)
  set(BUILD_CLI off)
endif()

# Release
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  message(STATUS "Using the Release build type")

  # Check compatibility
  if(BUILD_PYTHON OR
      UNIT_TESTS OR
      STATIC_ANALYSIS OR
      ENABLE_CODE_COVERAGE OR
      ENABLE_PYTHON_UNIT_TESTS OR
      ENABLE_SANITIZING)
    message(FATAL_ERROR "Specified options are incompatible with 'Release'")
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

# ReleasePython
if(CMAKE_BUILD_TYPE STREQUAL "ReleasePython")
  message(STATUS "Using the ReleasePython build type")

  # Check compatibility
  if(UNIT_TESTS OR
      STATIC_ANALYSIS OR
      ENABLE_CODE_COVERAGE OR
      ENABLE_PYTHON_UNIT_TESTS OR
      ENABLE_SANITIZING)
    message(FATAL_ERROR
      "Specified options are incompatible with 'ReleasePython'")
  endif()

  # Inherit compile flags from 'Release'
  separate_arguments(FLAGS NATIVE_COMMAND ${CMAKE_CXX_FLAGS_RELEASE})
  list(APPEND GLOBAL_COMPILE_OPTIONS ${FLAGS})
  separate_arguments(FLAGS NATIVE_COMMAND ${CMAKE_EXE_LINKER_FLAGS_RELEASE})
  list(APPEND GLOBAL_LINKER_OPTIONS ${FLAGS})

  # Options
  set(BUILD_PYTHON on)
  set(BUILD_CLI off)
  set(PACKAGE_PYTHON on)
  set(ENABLE_LTO on)
  set(USE_STATIC_LIBS on)
  set(CONFIGURE_FOR_DISTRIBUTION on)
  set(STRIP_PATHS_FROM_BINARIES on)
  set(STRIP_SUB_LIST "")
endif()

# Test
if(CMAKE_BUILD_TYPE STREQUAL "Test")
  message(STATUS "Using the Test build type")

  # Options on
  set(ENABLE_UNIT_TESTS on)
  set(BUILD_CLI on)

  # Options off
  set(CONFIGURE_FOR_DISTRIBUTION off)
  set(STRIP_PATHS_FROM_BINARIES off)
endif()

# TestPython
if(CMAKE_BUILD_TYPE STREQUAL "TestPython")
  message(STATUS "Using the 'TestPython' build type")

  # Options on
  set(BUILD_CLI off)
  set(BUILD_PYTHON on)
  set(ENABLE_PYTHON_UNIT_TESTS on)

  # Options off
  set(CONFIGURE_FOR_DISTRIBUTION off)
  set(STRIP_PATHS_FROM_BINARIES off)
endif()

# StaticAnalysis
if(CMAKE_BUILD_TYPE STREQUAL "StaticAnalysis")
  message(STATUS "Using the 'StaticAnalysis' build type")

  # If no specific static analysis is requested, do them all
  if(NOT RUN_CPPCHECK AND NOT RUN_IWYU AND
     NOT RUN_CPPLINT  AND NOT RUN_CLANG_TIDY)
    set(RUN_CPPCHECK on)
    set(RUN_IWYU on)
    set(RUN_CPPLINT on)
    set(RUN_CLANG_TIDY on)
  endif()

  set(STATIC_ANALYSIS_CHECKS "")
  if(RUN_CPPCHECK)
    list(APPEND STATIC_ANALYSIS_CHECKS "cppcheck")
  endif()
  if(RUN_CPPLINT)
    list(APPEND STATIC_ANALYSIS_CHECKS "cpplint")
  endif()
  if(RUN_IWYU)
    list(APPEND STATIC_ANALYSIS_CHECKS "iwyu")
  endif()
  if(RUN_CLANG_TIDY)
    list(APPEND STATIC_ANALYSIS_CHECKS "clang-tidy")
  endif()

  message(STATUS "Requested static analysis: ${STATIC_ANALYSIS_CHECKS}")

  # Options on
  set(BUILD_PYTHON off)  ## Until we can wrangle nanobind
  set(ENABLE_UNIT_TESTS on)
  set(ENABLE_STATIC_ANALYSIS on)

  # Options off
  set(CONFIGURE_FOR_DISTRIBUTION off)
  set(STRIP_PATHS_FROM_BINARIES off)
  set(STRIP_SUB_LIST "")
endif()
