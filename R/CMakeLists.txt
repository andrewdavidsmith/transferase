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

message(STATUS "Configuring to build R API")

# If the build is configured like this:
#
# cmake -B build -DCMAKE_CXX_COMPILER=g++ -DBUILD_R=on \
#     -DCMAKE_INSTALL_PREFIX:PATH=r_src -DCMAKE_BUILD_TYPE=Build
#
# And then installed like this (without a '--build' step):
#
# cmake --install build
#
# Then the following can install the package:
#
# R CMD INSTALL r_src
#
# And this will make the tarball that can be installed with
# 'install.packages':
#
# R CMD build r_src
#
# will produce a file like: 'transferase_0.5.0.tar.gz' which will have the
# same name as a file from 'cpack -B build --config build/CPackConfig.cmake'

# Copy the relevant sources from 'lib' into a source tree for the R extension.
install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/lib/
  DESTINATION src
  FILES_MATCHING PATTERN "*.*pp" PATTERN "LICENSE"
  PATTERN "tests" EXCLUDE
)

# Clone the 'asio' sources (into the build directory). This happens at
# configure time and not at build time.
include(FetchContent)
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/andrewdavidsmith/asio.git
  GIT_TAG main
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  SYSTEM
)
FetchContent_MakeAvailable(asio)
FetchContent_GetProperties(asio SOURCE_DIR ASIO_SOURCE_DIR)

# Install the 'asio' sources in the R extension tree.
install(
  DIRECTORY ${ASIO_SOURCE_DIR}/
  DESTINATION src/
  FILES_MATCHING PATTERN "*.*pp" PATTERN "LICENSE"
  PATTERN ".git" EXCLUDE
)

# Install the R-specific sources into the R extension tree.
install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/R/src/
  DESTINATION src/
  FILES_MATCHING PATTERN "*.*pp"
)

# Put the Makevars in place in the R extension tree.
install(
  FILES ${PROJECT_SOURCE_DIR}/R/src/Makevars
  DESTINATION src
)

# Configure and install the 'config.h' in the R extension tree.
configure_file(${PROJECT_SOURCE_DIR}/config.h.in config.h)
install(FILES ${PROJECT_BINARY_DIR}/config.h DESTINATION src)

# Put the R packaging files in place.
install(
  FILES
  ${PROJECT_SOURCE_DIR}/R/R/DESCRIPTION
  ${PROJECT_SOURCE_DIR}/R/R/NAMESPACE
  DESTINATION .
)

# Copy the 'zzz.R' into the R extension tree; this loads the module.
install(
  FILES
  ${PROJECT_SOURCE_DIR}/R/R/zzz.R
  DESTINATION R
)

# Copy the transferase system configuration file into a location where R will
# install so it can be found when doing transferase configuration.
set(DATA_ORIGIN_DIR ${PROJECT_BINARY_DIR}/share/transferase)
install(FILES ${DATA_ORIGIN_DIR}/transferase_data_${PROJECT_VERSION}.json
  DESTINATION inst
)

# The code below is to allow move-only data to have defaulted copy
# constructors because Rcpp seems to have no way to avoid arbitrary copies of
# things. The files that need their default constructors exposed must be
# listed below.
install(CODE [[
message(STATUS "Defaulting deleted constructors for R API")
set(FILES_TO_TRANSFORM
  "${CMAKE_INSTALL_PREFIX}/src/genome_index_data.hpp"
  "${CMAKE_INSTALL_PREFIX}/src/query_container.hpp"
  "${CMAKE_INSTALL_PREFIX}/src/genome_index.hpp"
)
foreach(cpp_file ${FILES_TO_TRANSFORM})
  message(STATUS "Transforming: ${cpp_file}")
  file(READ ${cpp_file} FILE_CONTENTS)
  string(REPLACE " = delete" " = default" FILE_CONTENTS "${FILE_CONTENTS}")
  file(WRITE ${cpp_file} "${FILE_CONTENTS}")
endforeach()
]])
