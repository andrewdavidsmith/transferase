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

add_library(utilities OBJECT
  utilities.hpp
  utilities.cpp)

add_library(lookup OBJECT
  lookup.hpp
  lookup.cpp)

add_library(logger OBJECT
  logger.hpp
  logger.cpp)

add_library(bins OBJECT
  bins.hpp
  bins.cpp)

add_library(zip OBJECT
  zip.hpp
  zip.cpp)

add_library(compress OBJECT
  compress.hpp
  compress.cpp)

target_include_directories(compress PRIVATE "${PROJECT_BINARY_DIR}")

add_library(check OBJECT
  check.hpp
  check.cpp)

add_library(merge OBJECT
  merge.hpp
  merge.cpp)

add_library(index OBJECT
  index.hpp
  index.cpp)

add_library(server_interface OBJECT
  server_interface.hpp
  server_interface.cpp)

add_library(server OBJECT
  server.hpp
  server.cpp)

add_library(connection OBJECT
  connection.hpp
  connection.cpp)

add_library(request_handler OBJECT
  request_handler.hpp
  request_handler.cpp)

add_library(methylome OBJECT
  methylome.hpp
  methylome.cpp)

add_library(methylome_set OBJECT
  methylome_set.hpp
  methylome_set.cpp)

add_library(cpg_index OBJECT
  cpg_index.hpp
  cpg_index.cpp)

add_library(cpg_index_set OBJECT
  cpg_index_set.hpp
  cpg_index_set.cpp)

add_library(genomic_interval OBJECT
  genomic_interval.hpp
  genomic_interval.cpp)

add_library(response OBJECT
  response.hpp
  response.cpp)

add_library(request OBJECT
  request.hpp
  request.cpp)

add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wfatal-errors)

add_executable(mxe)
target_include_directories(mxe PRIVATE "${PROJECT_BINARY_DIR}")
target_sources(mxe PRIVATE mxe.cpp)

if(MXE_BUILD_STATIC)
  target_link_libraries(mxe PRIVATE -static)
endif()

target_link_libraries(
  mxe PRIVATE
  utilities
  logger
  index
  compress
  check
  zip
  merge
  lookup
  server_interface
  bins
  genomic_interval
  cpg_index
  cpg_index_set
  methylome
  methylome_set
  server
  connection
  request_handler
  request
  response
  ZLIB::ZLIB
  Boost::program_options
  Threads::Threads
)
install(TARGETS mxe DESTINATION bin)
