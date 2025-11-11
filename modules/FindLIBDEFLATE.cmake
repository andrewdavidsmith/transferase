# MIT License
#
# Copyright (c) 2025 Andrew Smith
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

# FindLIBDEFLATE.cmake
# Custom CMake module to find libdeflate

# Support preference of static libs by adjusting CMAKE_FIND_LIBRARY_SUFFIXES
# ADS: this is take from the FindBoost.cmake file
if(LIBDEFLATE_USE_STATIC_LIBS)
  set(_libdeflate_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES
    ${CMAKE_FIND_LIBRARY_SUFFIXES}
  )
  if(WIN32)
    list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 .lib .a)
  else()
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
  endif()
endif()

find_path(LIBDEFLATE_INCLUDE_DIR NAMES libdeflate.h)
find_library(LIBDEFLATE_LIBRARY NAMES deflate libdeflate)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBDEFLATE
  REQUIRED_VARS LIBDEFLATE_LIBRARY LIBDEFLATE_INCLUDE_DIR
  VERSION_VAR LIBDEFLATE_VERSION
)

if(LIBDEFLATE_FOUND AND NOT TARGET LIBDEFLATE::LIBDEFLATE)
  add_library(LIBDEFLATE::LIBDEFLATE UNKNOWN IMPORTED)
  set_target_properties(LIBDEFLATE::LIBDEFLATE PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${LIBDEFLATE_INCLUDE_DIR}"
    IMPORTED_LOCATION "${LIBDEFLATE_LIBRARY}"
  )
endif()

# Restore the original find library ordering
# ADS: this is take from the FindBoost.cmake file
if(LIBDEFLATE_USE_STATIC_LIBS)
  set(CMAKE_FIND_LIBRARY_SUFFIXES
    ${_libdeflate_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES}
  )
endif()
