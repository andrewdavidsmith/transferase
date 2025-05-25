# MIT License
#
# Copyright (c) 2024-2025 Andrew Smith
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

# Test sources in this directory need to see the headers from the
# project sources -- they should get them by linking to the libs

message(STATUS "Enabling unit tests for cli")

enable_testing()
include(GoogleTest)

add_library(unit_test_utils_cli OBJECT unit_test_utils_cli.cpp unit_test_utils_cli.hpp)

file(GLOB cpp_files "*_test.cpp")

# Ensure all objects depend on both the cpp and hpp files
foreach(cpp_file ${cpp_files})
  get_filename_component(BASE_NAME ${cpp_file} NAME_WE)
  add_executable(${BASE_NAME} ${cpp_file})
  target_link_libraries(${BASE_NAME} PRIVATE
    GTest::gtest
    GTest::gtest_main
    transferase_system
    transferase_cli
    unit_test_utils_cli
  )
  # Make sure 'config.h' is available and headers in 'lib' we are testing.
  target_include_directories(${BASE_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/lib
  )
  target_link_options(${BASE_NAME} PRIVATE ${GLOBAL_LINKER_OPTIONS})
  target_compile_options(${BASE_NAME} PRIVATE ${GLOBAL_COMPILE_OPTIONS})
  # Discover all tests in these files
  gtest_discover_tests(${BASE_NAME})
endforeach()

## Ensure test data dir is in the right place relative to ctest paths
# Do this in the current CMakeLists.txt because beforehand the target
# dir might not exit.
file(CREATE_LINK
  ${PROJECT_SOURCE_DIR}/test_data
  ${PROJECT_BINARY_DIR}/cli/tests/data
  SYMBOLIC
)

message(STATUS
  "Configuring data files for lib tests (up one level: 'bins' sibling 'share')"
)

## DATADIR is used within the sources, taken from config.h where it is
## assigned during the build config
set(DATADIR ${CMAKE_INSTALL_DATADIR})

# Configure this dir and JSON file for any build. They are always needed in
# the right place with the version in the filename. Notice the '..' which puts
# the data beside the binaries and not in the same directory.
set(BUILD_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR}/../${DATADIR}/transferase)
file(MAKE_DIRECTORY ${BUILD_DATA_DIR})
configure_file(${PROJECT_SOURCE_DIR}/data/transferase_data.json
  ${BUILD_DATA_DIR}/transferase_data_${PROJECT_VERSION}.json
  COPYONLY
)
configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
