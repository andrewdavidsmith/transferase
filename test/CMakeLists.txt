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

cmake_minimum_required(VERSION 3.30)

add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wfatal-errors)

include_directories(${PROJECT_SOURCE_DIR}/src)

find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(cpg_index_meta_test cpg_index_meta_test.cpp)
target_link_libraries(cpg_index_meta_test
 PRIVATE
 GTest::GTest
 GTest::Main
 Boost::json
 utilities
 cpg_index_meta
)

add_executable(cpg_index_test cpg_index_test.cpp)
target_link_libraries(cpg_index_test
 PRIVATE
 GTest::GTest
 GTest::Main
 Boost::json
 ZLIB::ZLIB
 Threads::Threads
 utilities
 cpg_index_meta
 cpg_index
)

add_executable(methylome_test methylome_test.cpp)
target_link_libraries(methylome_test
 PRIVATE
 GTest::GTest
 GTest::Main
 ZLIB::ZLIB
 methylome_metadata
 methylome
 utilities
)

add_executable(counts_file_formats_test counts_file_formats_test.cpp)
target_link_libraries(counts_file_formats_test
 PRIVATE
 GTest::GTest
 GTest::Main
 ZLIB::ZLIB
 counts_file_formats
 zlib_adapter
)

add_executable(genomic_interval_test genomic_interval_test.cpp)
target_link_libraries(genomic_interval_test
 PRIVATE
 GTest::GTest
 GTest::Main
 Boost::json
 ZLIB::ZLIB
 genomic_interval
 utilities
 hash
 cpg_index
 cpg_index_meta
)

add_executable(request_test request_test.cpp)
target_link_libraries(request_test
 PRIVATE
 GTest::GTest
 GTest::Main
 request
)

add_executable(methylome_set_test methylome_set_test.cpp)
target_link_libraries(methylome_set_test
 PRIVATE
 GTest::GTest
 GTest::Main
 ZLIB::ZLIB
 methylome
 utilities
 methylome_metadata
 methylome_set
)

add_executable(request_handler_test request_handler_test.cpp)
target_link_libraries(request_handler_test
 PRIVATE
 GTest::GTest
 GTest::Main
 ZLIB::ZLIB
 request_handler
 request
 logger
 methylome
 utilities
 methylome_metadata
 methylome_set
 cpg_index
 cpg_index_meta
 cpg_index_set
)

add_executable(command_config_argset_test command_config_argset_test.cpp)
target_link_libraries(command_config_argset_test
 PRIVATE
 GTest::GTest
 GTest::Main
 Boost::boost
 Boost::json
 Boost::program_options
 utilities
)

add_executable(command_config_test command_config_test.cpp)
target_link_libraries(command_config_test
 PRIVATE
 GTest::GTest
 GTest::Main
 Boost::boost
 Boost::json
 Boost::context
 Boost::program_options
 utilities
 logger
 download
 command_config
)

set(EXECUTABLE_TARGETS
  cpg_index_meta_test
  counts_file_formats_test
  cpg_index_test
  methylome_test
  methylome_set_test
  genomic_interval_test
  request_test
  request_handler_test
  command_config_argset_test
  command_config_test
)

# Define the UNIT_TEST macro for the test targets
foreach(target ${EXECUTABLE_TARGETS})
  target_compile_definitions(${target} PRIVATE UNIT_TEST)
endforeach()

if(MXE_USE_STATIC_LIBS)
  add_library(static_target INTERFACE)
  target_link_options(static_target INTERFACE -static-libgcc -static-libstdc++)
  foreach(target ${EXECUTABLE_TARGETS})
    target_link_libraries(${target} PRIVATE static_target)
  endforeach()
endif()

foreach(target ${EXECUTABLE_TARGETS})
  gtest_discover_tests(${target})
endforeach()

# Add compile definitions to specific targets
file(CREATE_LINK
  ${PROJECT_SOURCE_DIR}/test/data
  ${PROJECT_BINARY_DIR}/test/data
  SYMBOLIC
)
