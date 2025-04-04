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
  # Typically ZLib isn't built with -fPIC, but it's small so we can
  # clone and build it if needed
  message(STATUS "Configuring to clone ZLib for Python API")
  include(ExternalProject)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    ExternalProject_Add(
      ZLIB
      GIT_REPOSITORY https://github.com/madler/zlib.git
      GIT_TAG master
      CMAKE_ARGS
      -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
      -DZLIB_BUILD_EXAMPLES=off
      -DSKIP_INSTALL_FILES=on
      -DCMAKE_POSITION_INDEPENDENT_CODE=on
      -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}/src/zlib
      -DCMAKE_BUILD_TYPE=Release
    )
  else()
    ExternalProject_Add(
      ZLIB
      GIT_REPOSITORY https://github.com/madler/zlib.git
      GIT_TAG master
      CMAKE_ARGS
      -DZLIB_BUILD_EXAMPLES=off
      -DSKIP_INSTALL_FILES=on
      -DCMAKE_POSITION_INDEPENDENT_CODE=on
      -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}/src/zlib
      -DCMAKE_BUILD_TYPE=Release
    )
  endif()
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

# Python and nanobind
if(BUILD_PYTHON)
  message(STATUS "Configuring nanobind for Python API")

  ## Try to find nanobind on the system first
  # For nanobind before Python; seems to work with ordinary
  # 'Development' but Development.Module is recommended
  set(DEV_MODULE Development.Module)

  # Python has to be available before nanobind
  find_package(Python COMPONENTS Interpreter ${DEV_MODULE} REQUIRED)
  message(STATUS "Python include dir: ${Python_INCLUDE_DIRS}")
  message(STATUS "Python libraries: ${Python_LIBRARIES}")
  message(STATUS "Python version: ${Python_VERSION}")
  message(STATUS "Python SOABI: ${Python_SOABI}")

  # Detect the installed nanobind package
  execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE nanobind_ROOT
    ERROR_QUIET
  )
  if (nanobind_ROOT)
    message(STATUS "Found nanobind on the system")
  else()
    message(STATUS "Configuring to clone nanobind")
    # Didn't find nanobind so try to clone it
    include(ExternalProject)
    ExternalProject_Add(
      nanobind
      GIT_REPOSITORY https://github.com/wjakob/nanobind.git
      GIT_TAG master
      CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}/src/nanobind
      -DCMAKE_BUILD_TYPE=Release
    )
  endif()
  # Now try to import nanobind; if this step is not done here, then in
  # python/CMakeLists.txt the
  # nanobind_build_library(nanobind-static-abi3) will fail.
  find_package(nanobind CONFIG)
endif()
