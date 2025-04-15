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

message(STATUS "Locating REQUIRED third-party packages")

# ZLib
if(BUILD_PYTHON)
  # Typically ZLib isn't built with -fPIC, but it's small so we
  # clone and build it with POSITION_INDEPENDENT_CODE=on here
  message(STATUS "Configuring to clone ZLib for Python API")
  include(ExternalProject)
  set(ZLIB_CMAKE_ARGS
    -DZLIB_BUILD_EXAMPLES=off
    -DSKIP_INSTALL_FILES=on
    -DCMAKE_POSITION_INDEPENDENT_CODE=on
    -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}/src/zlib
    -DCMAKE_BUILD_TYPE=Release
  )
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND ZLIB_CMAKE_ARGS
      -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
    )
  endif()
  ExternalProject_Add(
    ZLIB
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG master
    CMAKE_ARGS ${ZLIB_CMAKE_ARGS}
  )
  # Include the built zlib headers and link against the built zlib library
  set(ZLIB_INCLUDE_DIR "${PROJECT_BINARY_DIR}/src/zlib/include")
  set(ZLIB_LIBRARY "${PROJECT_BINARY_DIR}/src/zlib/lib/libz.a")

  # Create the CMake target for the built zlib
  add_library(ZLIB_IMPORTED INTERFACE)
  set_target_properties(ZLIB_IMPORTED PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${ZLIB_INCLUDE_DIR}
    INTERFACE_LINK_LIBRARIES ${ZLIB_LIBRARY}
  )

  # This alias means we don't need to worry where zlib came from
  add_library(ZLIB::ZLIB ALIAS ZLIB_IMPORTED)
else()
  find_package(ZLIB REQUIRED)
endif()
