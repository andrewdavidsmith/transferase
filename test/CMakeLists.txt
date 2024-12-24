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

find_package(GTest REQUIRED)
include(GoogleTest)

include_directories(${PROJECT_SOURCE_DIR}/src)

add_executable(zlib_adapter_test zlib_adapter_test.cpp)
target_link_libraries(zlib_adapter_test
 PRIVATE
 GTest::GTest
 GTest::Main
 ZLIB::ZLIB
 zlib_adapter
 utilities
)

add_executable(download_test download_test.cpp)
target_link_libraries(download_test
 PRIVATE
 GTest::GTest
 GTest::Main
 Boost::boost
 Boost::context
 download
 utilities
)

add_executable(cpg_index_metadata_test cpg_index_metadata_test.cpp)
target_link_libraries(cpg_index_metadata_test
 PRIVATE
 GTest::GTest
 GTest::Main
 Boost::json
 utilities
 cpg_index_metadata
)
target_include_directories(cpg_index_metadata_test PRIVATE "${PROJECT_BINARY_DIR}")

add_executable(cpg_index_data_test cpg_index_data_test.cpp)
target_link_libraries(cpg_index_data_test
 PRIVATE
 GTest::GTest
 GTest::Main
 Boost::json
 ZLIB::ZLIB
 Threads::Threads
 utilities
 cpg_index_metadata
 cpg_index_data
 cpg_index
 logger
 zlib_adapter
)

add_executable(methylome_data_test methylome_data_test.cpp)
target_link_libraries(methylome_data_test
 PRIVATE
 GTest::GTest
 GTest::Main
 ZLIB::ZLIB
 methylome_metadata
 methylome_data
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
 logger
 cpg_index
 cpg_index_data
 cpg_index_metadata
 zlib_adapter
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
 methylome_data
 utilities
 methylome_metadata
 methylome_set
 methylome
 logger
)

add_executable(cpg_index_set_test cpg_index_set_test.cpp)
target_link_libraries(cpg_index_set_test
 PRIVATE
 GTest::GTest
 GTest::Main
 Boost::json
 ZLIB::ZLIB
 cpg_index
 zlib_adapter
 logger
 utilities
 cpg_index_metadata
 cpg_index_set
 cpg_index_data
 cpg_index
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
 methylome_data
 methylome
 utilities
 methylome_metadata
 genomic_interval
 methylome_set
 cpg_index
 cpg_index_data
 cpg_index_metadata
 cpg_index_set
 zlib_adapter
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

add_executable(command_intervals_test command_intervals_test.cpp)
target_link_libraries(command_intervals_test
 PRIVATE
 GTest::GTest
 GTest::Main
 ZLIB::ZLIB
 Boost::boost
 Boost::json
 Boost::program_options
 cpg_index
 cpg_index_data
 cpg_index_metadata
 genomic_interval
 logger
 methylome
 methylome_data
 methylome_metadata
 request
 response
 utilities
 zlib_adapter
 command_intervals
)

set(EXECUTABLE_TARGETS
  zlib_adapter_test
  download_test
  cpg_index_set_test
  cpg_index_metadata_test
  counts_file_formats_test
  cpg_index_data_test
  methylome_data_test
  methylome_set_test
  genomic_interval_test
  request_test
  request_handler_test
  command_config_argset_test
  command_config_test
  command_intervals_test
)

# Define the UNIT_TEST macro for the test targets
foreach(target ${EXECUTABLE_TARGETS})
  target_compile_definitions(${target} PRIVATE UNIT_TEST)
endforeach()

foreach(target ${EXECUTABLE_TARGETS})
  gtest_discover_tests(${target})
endforeach()

# Ensure test data dir is in the right place relative to ctest paths
file(CREATE_LINK
  ${PROJECT_SOURCE_DIR}/test/data
  ${PROJECT_BINARY_DIR}/test/data
  SYMBOLIC
)
