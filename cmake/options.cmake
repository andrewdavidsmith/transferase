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

# configration options for buildling transferase

option(UNIT_TESTS "Enable unit tests" off)
option(PACKAGE "Enable packaging for release" off)
option(STATIC_ANALYSIS "Enable static analysis" off)
option(STATIC_LIBS "Use static linkage for non-system libs" off)
option(SANITIZE "Enable sanitizing" off)
option(CODE_COVERAGE "Enable code coverage" off)
option(LTO "Enable link-time optimization" off)
option(BUILD_PYTHON "Build Python bindings" off)
option(PACKAGE_PYTHON "Package the Python bindings" off)
option(PYTHON_TESTS "Enable Python tests" off)
option(LIB_ONLY "Skip building the cli" off)
set(SANITIZER_TYPE "address"
  CACHE STRING "Choose sanitizer type (address, undefined)")

if(TEST_LIB OR TEST_CLI)
  set(UNIT_TESTS on)
endif()

set(BUILD_CLI on)
if(LIB_ONLY OR BUILD_PYTHON OR PACKAGE_PYTHON)
  set(BUILD_CLI off)
endif()

# Release
if(PACKAGE)
  message(STATUS "Enabling 'PACKAGE'")
  # Check compatibility
  if(BUILD_PYTHON OR
      UNIT_TESTS OR
      PYTHON_TESTS OR
      STATIC_ANALYSIS OR
      ENABLE_CODE_COVERAGE OR
      SANITIZE)
    message(FATAL_ERROR
      "Specified options incompatible with 'PACKAGE'")
  endif()
  # Options
  set(BUILD_CLI on)
  set(USE_STATIC_LIBS on)
  set(ENABLE_LTO on)
  set(STRIP_PATHS_FROM_BINARIES on)
  set(STRIP_SUB_LIST "")
endif()

# Python
if(BUILD_PYTHON)
  message(STATUS "Enabling 'BUILD_PYTHON'")
  # Check compatibility
  if(UNIT_TESTS OR
      BUILD_CLI OR
      ENABLE_CODE_COVERAGE OR
      SANITIZE)
    message(FATAL_ERROR
      "Specified options incompatible with 'BUILD_PYTHON'")
  endif()
  # Options
  set(STRIP_PATHS_FROM_BINARIES on)
  set(STRIP_SUB_LIST "")
endif()

# Python
if(PACKAGE_PYTHON)
  message(STATUS "Enabling 'PACKAGE_PYTHON'")
  # Check compatibility
  if(UNIT_TESTS OR
      PACKAGE OR
      BUILD_CLI OR
      STATIC_ANALYSIS OR
      ENABLE_CODE_COVERAGE OR
      SANITIZE)
    message(FATAL_ERROR
      "Specified options incompatible with 'PACKAGE_PYTHON'")
  endif()
  # Options on
  set(BUILD_PYTHON on)
  set(ENABLE_LTO on)
  set(USE_STATIC_LIBS on)
  set(STRIP_PATHS_FROM_BINARIES on)
  set(STRIP_SUB_LIST "")
endif()

# Tests
if(UNIT_TESTS)
  message(STATUS "Enabling 'UNIT_TESTS'")
  # If no specific tests requested, do them all
  if(NOT TEST_LIB AND NOT TEST_CLI)
    set(TEST_LIB on)
    set(TEST_CLI on)
  endif()
  # Options on
  if(TEST_CLI)
    set(BUILD_CLI on)
  endif()
endif()

# PythonTests
if(PYTHON_TESTS)
  message(STATUS "Enabling 'PYTHON_TESTS'")
  # Check compatibility
  if(UNIT_TESTS OR
      BUILD_CLI OR
      STATIC_ANALYSIS OR
      ENABLE_CODE_COVERAGE OR
      SANITIZE)
    message(FATAL_ERROR
      "Specified options incompatible with 'PYTHON_TESTS'")
  endif()
  # Options on
  set(BUILD_PYTHON on)
  # Options off
  set(TEST_LIB off)
  set(TEST_LIB off)
  set(TEST_CLI off)
  set(BUILD_CLI off)
  set(STRIP_PATHS_FROM_BINARIES off)
endif()

# StaticAnalysis
if(STATIC_ANALYSIS)
  message(STATUS "Enabling static analysis")
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
  set(UNIT_TESTS on)
  # Options off
  set(STRIP_PATHS_FROM_BINARIES off)
  set(STRIP_SUB_LIST "")
endif()
