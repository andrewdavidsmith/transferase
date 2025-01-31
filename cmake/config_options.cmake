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

option(ENABLE_UNIT_TESTS "Enable unit tests" off)
option(ENABLE_STATIC_ANALYSIS "Enable static analysis" off)
option(USE_STATIC_LIBS "Use static linkage for non-system libs" off)
option(ENABLE_SANITIZING "Enable sanitizing" off)
option(ENABLE_CODE_COVERAGE "Enable code coverage analysis" off)
option(ENABLE_LTO "Enable link-time optimization" off)
option(BUILD_PYTHON "Build Python bindings" off)
option(PACKAGE_PYTHON "Package the Python bindings" off)
option(ENABLE_PYTHON_UNIT_TESTS "Enable Python unit tests" off)
set(SANITIZER_TYPE "address"
  CACHE STRING "Choose sanitizer type (address, undefined)")

# Make ENABLE_PYTHON_UNIT_TESTS depend on BUILD_PYTHON
if(NOT BUILD_PYTHON)
  set(ENABLE_PYTHON_UNIT_TESTS OFF CACHE BOOL "Enable Python unit tests" FORCE)
endif()
