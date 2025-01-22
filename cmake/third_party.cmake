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

find_package(Threads REQUIRED)

set(BOOST_COMPONENTS program_options context)

# Boost.JSON
if(BUILD_PYTHON)
  ## Building python lib needs -fPIC and Boost.json has an issue with
  ## this so BOOST_JSON_NO_LIB will be passed to sources and turn on
  ## header-only for Boost.json
  set(BOOST_JSON_NO_LIB on)
else()
  list(APPEND BOOST_COMPONENTS json)
endif()

# ZLib
if(BUILD_PYTHON)
  # Typically ZLib isn't built with -fPIC, but it's small so we can
  # clone and build it if needed
  message(STATUS "Configuring to clone ZLib for Python API")
  include(ExternalProject)
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

# Python and Pybind11
if(BUILD_PYTHON)
  # Python3
  find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
  message(STATUS "Python3 include dir: ${Python3_INCLUDE_DIRS}")
  message(STATUS "Python3 libraries: ${Python3_LIBRARIES}")
  message(STATUS "Python3 version: ${Python3_VERSION}")
  message(STATUS "Python3 SOABI: ${Python3_SOABI}")

  # Pybind11
  find_package(pybind11 REQUIRED)
endif()

# Boost components
find_package(
  Boost
  1.85.0 REQUIRED
  COMPONENTS
  ${BOOST_COMPONENTS}
)
message(STATUS "Boost version: ${Boost_VERSION_STRING}")
message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost library dirs: ${Boost_LIBRARY_DIRS}")

# Optional libraries
message(STATUS "Locating OPTIONAL third-party packages")

# Curses
include(FindCurses)  # This should be in the modules dir
set(CURSES_NEED_NCURSES TRUE)
find_package(Curses)
if(CURSES_FOUND AND CURSES_HAVE_NCURSES_H)
  ## ADS: above, the header used in the sources is <ncurses.h> and not
  ## <curses.h> or <ncurses/curses.h>
  add_compile_definitions(HAVE_NCURSES)
  message(STATUS "Found ncurses header: ${CURSES_HAVE_NCURSES_H}")
  message(STATUS "Found ncurses libraries: ${CURSES_LIBRARIES}")
else()
  message(STATUS "NCurses not found: 'select' command will not be built")
endif()
