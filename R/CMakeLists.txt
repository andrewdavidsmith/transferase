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

set(CMAKE_INSTALL_MESSAGE NEVER)

message(STATUS "Configuring to build R API")

# If the build is configured like this:
#
# cmake -B build -DBUILD_R=on -DCMAKE_INSTALL_PREFIX=Rxfr
#
# And then installed like this (without a '--build' step):
#
# cmake --install build
#
# Then the following can install the package:
#
# R CMD INSTALL Rxfr
#
# And this will make the tarball that can be installed with
# 'install.packages':
#
# R CMD build Rxfr
#
# will produce a file like: 'Rxfr_0.6.4.tar.gz' which will have the
# same name as a file from 'cpack -B build --config build/CPackConfig.cmake'

# Check that required R packages have been installed
set(REQUIRED_R_PACKAGES "Rcpp;R6")
foreach(package_name ${REQUIRED_R_PACKAGES})
  message(STATUS "Checking R package: ${package_name}")
  # ADS: Only works if Rscript works
  execute_process(
    COMMAND Rscript -e "library(${package_name})"
    RESULT_VARIABLE EXIT_CODE
    OUTPUT_QUIET
  )
  if(${EXIT_CODE})
    message(FATAL_ERROR "Required R package not found: ${package_name}")
  endif()
endforeach()

# Copy the relevant sources from 'lib' into a source tree for the R extension.
install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/lib/
  DESTINATION src
  FILES_MATCHING PATTERN "*.*pp"
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
  FILES_MATCHING PATTERN "*.*pp"
  PATTERN ".git" EXCLUDE
)

# Install the R-specific sources into the R extension tree.
install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/R/src/
  DESTINATION src/
  FILES_MATCHING PATTERN "*.*pp"
)

# Put the files required for building by R in place in the R extension tree.
install(
  FILES
  ${PROJECT_SOURCE_DIR}/R/configure.ac
  ${PROJECT_SOURCE_DIR}/R/.Rbuildignore
  DESTINATION .
)
install(
  FILES
  ${PROJECT_SOURCE_DIR}/R/src/Makevars.in
  DESTINATION src
)
# Put the m4 scripts directory in place in the R extension tree.
install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/R/m4
  DESTINATION .
)

# Configure and install the 'config.h' in the R extension tree.
configure_file(${PROJECT_SOURCE_DIR}/config.h.in config.h)
install(FILES ${PROJECT_BINARY_DIR}/config.h DESTINATION src)

# Put the R packaging files in place.
install(
  FILES
  ${PROJECT_SOURCE_DIR}/R/DESCRIPTION
  ${PROJECT_SOURCE_DIR}/R/NAMESPACE
  DESTINATION .
)

# Copy the 'zzz.R' into the R extension tree; this loads the module.
install(
  FILES
  ${PROJECT_SOURCE_DIR}/R/R/zzz.R
  ${PROJECT_SOURCE_DIR}/R/R/exports.R
  DESTINATION R
)

# Copy the LICENSE stub into the root of the R package dir.
install(
  FILES
  ${PROJECT_SOURCE_DIR}/R/LICENSE_STUB
  DESTINATION .
  RENAME LICENSE
)

# Copy the full LICENSE file into the inst directory of the R package.
install(
  FILES
  ${PROJECT_SOURCE_DIR}/data/LICENSE
  DESTINATION inst
  RENAME LICENSE.note
)

# Copy the R documentation into the man dir of the package.
install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/R/man/
  DESTINATION man/
  FILES_MATCHING PATTERN "*.Rd"
)

# Copy the transferase system configuration file into a location where R will
# install so it can be found when doing transferase configuration.
set(DATA_ORIGIN_DIR ${PROJECT_BINARY_DIR}/share/transferase)
install(FILES ${DATA_ORIGIN_DIR}/transferase_data_${PROJECT_VERSION}.json
  DESTINATION inst
)

# The code below is executed at install time after the 'install' commands
# above have put files into place.

# Transform code to allow move-only data to have defaulted copy constructors
# because Rcpp seems to have no way to avoid arbitrary copies of things. The
# files that need their default constructors exposed must be listed below.
install(CODE [[
message(STATUS "Defaulting deleted constructors for R API")
set(FILES_TO_TRANSFORM
  "${CMAKE_INSTALL_PREFIX}/src/genome_index_data.hpp"
  "${CMAKE_INSTALL_PREFIX}/src/query_container.hpp"
  "${CMAKE_INSTALL_PREFIX}/src/genome_index.hpp"
)
foreach(cpp_file ${FILES_TO_TRANSFORM})
  file(READ ${cpp_file} FILE_CONTENTS)
  string(REPLACE " = delete" " = default" FILE_CONTENTS "${FILE_CONTENTS}")
  file(WRITE ${cpp_file} "${FILE_CONTENTS}")
endforeach()
]])

# Replace the .hpp with .h for all include statements because of the R
# extensions guide...
install(CODE [[
message(STATUS "Replacing .hpp with .h in includes")
file(GLOB_RECURSE CXX_FILES ${CMAKE_INSTALL_PREFIX}/*.*pp)
foreach(cxx_file ${CXX_FILES})
  file(READ ${cxx_file} FILE_CONTENTS)
  string(REPLACE ".hpp" ".h" FILE_CONTENTS "${FILE_CONTENTS}")
  string(REPLACE "_HPP_" "_H_" FILE_CONTENTS "${FILE_CONTENTS}")
  file(WRITE ${cxx_file} "${FILE_CONTENTS}")
endforeach()
]])

# And rename headers to .h for the same reason
install(CODE [[
message(STATUS "Renaming header from hpp extension to h")
file(GLOB_RECURSE HPP_FILES ${CMAKE_INSTALL_PREFIX}/*.hpp)
foreach(hpp_file ${HPP_FILES})
  string(REPLACE "hpp" "h" h_file ${hpp_file})
  file(RENAME ${hpp_file} ${h_file})
endforeach()
]])

# Remove files that are not relevant for client (a temporary hack)
install(CODE [[
message(STATUS "Removing unused files for client")
set(FILES_TO_REMOVE
  "${CMAKE_INSTALL_PREFIX}/src/connection.hpp"
  "${CMAKE_INSTALL_PREFIX}/src/connection.cpp"
  "${CMAKE_INSTALL_PREFIX}/src/server.cpp"
  "${CMAKE_INSTALL_PREFIX}/src/server.hpp"
  "${CMAKE_INSTALL_PREFIX}/src/request_handler.hpp"
  "${CMAKE_INSTALL_PREFIX}/src/request_handler.cpp"
  "${CMAKE_INSTALL_PREFIX}/src/bins_writer.cpp"
  "${CMAKE_INSTALL_PREFIX}/src/bins_writer.hpp"
  "${CMAKE_INSTALL_PREFIX}/src/counts_file_format.hpp"
  "${CMAKE_INSTALL_PREFIX}/src/counts_file_format.cpp"
  "${CMAKE_INSTALL_PREFIX}/src/counts_file_format_impl.hpp"
  "${CMAKE_INSTALL_PREFIX}/src/intervals_writer.hpp"
  "${CMAKE_INSTALL_PREFIX}/src/intervals_writer.cpp"
  "${CMAKE_INSTALL_PREFIX}/src/output_format_type.hpp"
  "${CMAKE_INSTALL_PREFIX}/src/output_format_type.cpp"
  "${CMAKE_INSTALL_PREFIX}/src/server_config.hpp"
  "${CMAKE_INSTALL_PREFIX}/src/server_config.cpp"
)
file(REMOVE ${FILES_TO_REMOVE})
file(REMOVE_RECURSE ${CMAKE_INSTALL_PREFIX}/src/asio/experimental)
]])

# Generate the Rcpp boilerplate Remove files that are not relevant for client
# (a temporary hack that is now not-so-temporary...)
install(CODE [[
message(STATUS "Generating RcppExports.cpp boilerplate")
execute_process(
  COMMAND
  Rscript -e "library(Rcpp); Rcpp::compileAttributes(\"${CMAKE_INSTALL_PREFIX}\")"
  RESULT_VARIABLE EXIT_CODE
)
execute_process(
  COMMAND
  rm ${CMAKE_INSTALL_PREFIX}/R/RcppExports.R
)
]])

install(CODE [[
message(STATUS "Adding include for transferase_r.hpp to RcppExports.cpp")
set(EXPORTS_FILENAME ${CMAKE_INSTALL_PREFIX}/src/RcppExports.cpp)
message(STATUS "EXPORTS_FILENAME: ${EXPORTS_FILENAME}")
execute_process(
  COMMAND
  sed -i.remove "1s/^/\#include \"transferase_r.h\"\\n/" ${EXPORTS_FILENAME}
  RESULT_VARIABLE EXIT_CODE
)
execute_process(
  COMMAND
  rm ${EXPORTS_FILENAME}.remove
)
]])

install(CODE [[
message(STATUS "Replacing std::cout and std::cerr with Rcpp equivalents")
message(STATUS "Replacing stdout/stderr with nullptr and removing std::fputs")
set(INDICATORS_FILENAME ${CMAKE_INSTALL_PREFIX}/src/indicators/indicators.h)
execute_process(
  COMMAND
  sed -i -E "1s/^/\#include <Rcpp.h>\\n/; \
  s/std::cout/Rcpp::Rcout/g; \
  s/std::cerr/Rcpp::Rcerr/g; \
  s/std::clog/Rcpp::Rcerr/g; \
  s/stdout/nullptr/g; \
  s/stderr/nullptr/g; \
  s/^ +std::fputs.*\$/{};/g"
  ${INDICATORS_FILENAME}
  RESULT_VARIABLE EXIT_CODE
)
]])
