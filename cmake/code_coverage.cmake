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

# Check for coverage support in the compiler
include(CheckCXXCompilerFlag)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")

# Try to add the necessary flags for coverage
check_cxx_compiler_flag("--coverage" COVERAGE_SUPPORTED)

if(COVERAGE_SUPPORTED)
  message(STATUS "Code coverage is supported by the compiler")
else()
  message(FATAL_ERROR "Code coverage is NOT supported by the compiler")
endif()

add_custom_target(code_coverage
  COMMENT "Running lcov and cleaning using exclusion patterns"
  COMMAND lcov
  --directory ${PROJECT_BINARY_DIR}
  --capture
  --output-file coverage-full.info
  COMMAND lcov
  --remove coverage-full.info '*_test.cpp' '*/include/*' '*automatic_json.hpp'
  '*unit_test_utils.*pp'
  --output-file ${PROJECT_BINARY_DIR}/coverage.info
  COMMAND
  ${CMAKE_COMMAND} -E remove coverage-full.info
  COMMAND
  genhtml
  ${PROJECT_BINARY_DIR}/coverage.info
  --output-directory ${PROJECT_BINARY_DIR}/lcov
)
