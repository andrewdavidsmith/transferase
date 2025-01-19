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

message(STATUS "Configuring data files needed by all builds")
if(NOT DEFINED CMAKE_INSTALL_DATADIR)
  message(FATAL_ERROR
    "CMAKE_INSTALL_DATADIR is needed for data configuration but is not defined"
  )
endif()

## DATADIR is used within the sources, taken from config.h where it is
## assigned during the build config
set(DATADIR "${CMAKE_INSTALL_DATADIR}")

# Configure this dir and JSON file for any build. They are always
# needed in the right place with the version in the filename
set(BUILD_DATA_DIR "${PROJECT_BINARY_DIR}/${DATADIR}/transferase")
file(MAKE_DIRECTORY ${BUILD_DATA_DIR})
configure_file(${CMAKE_SOURCE_DIR}/data/transferase_data.json
  ${BUILD_DATA_DIR}/transferase_data_${PROJECT_VERSION}.json
  COPYONLY
)

configure_file(config.h.in config.h)
