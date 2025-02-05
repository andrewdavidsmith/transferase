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

# transferase cmake file for packaging

# This file sets STRIP_SUB_LIST and GLOBAL_LINKER_OPTIONS options
# which are used by compiling and linking.

message(STATUS "Configuring to package for distribution")

set(CPACK_PACKAGE_VENDOR "Andrew D Smith")
set(CPACK_PACKAGE_CONTACT "Andrew D Smith")
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE)
set(CPACK_GENERATOR "TGZ;STGZ")

# ADS: adding this is the most general way to make the alias and have
# it both installed properly and linked the same way as tranferase.
set(BUILD_XFR_ALIAS on)

if (STRIP_PATHS_FROM_BINARIES)
  # Strip binaries for size and removing build system info. This can
  # take a bool, but also a list, so we can give it both transferase and xfr
  set(CPACK_STRIP_FILES on)

  # Remove project source directory information from text section
  list(APPEND STRIP_SUB_LIST "${PROJECT_SOURCE_DIR}/=/")
  # Replace any home dir prefix in paths
  list(APPEND STRIP_SUB_LIST "$ENV{HOME}/=/")
endif()

## Set linker to strips symbols (seems not redundant with cpack
## setting?)
# Check for the operating system
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")  # macOS
  list(APPEND GLOBAL_LINKER_OPTIONS "-Wl,-dead_strip")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND GLOBAL_LINKER_OPTIONS "-s")
elseif(CMAKE_SYSTEM_NAME STREQUAL "WIN32")
  message(FATAL_ERROR "Windows isn't supported")
else()
  message(FATAL_ERROR "Operatoring system not recognized: ${CMAKE_SYSTEM_NAME}")
endif()

# For custom STGZ generator
set(CPACK_STGZ_HEADER_FILE
  "${CMAKE_SOURCE_DIR}/modules/CPack.STGZ_Header.sh.in")

# Only make the Debian installer if building on Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND CPACK_GENERATOR "DEB")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Andrew D Smith <andrewds@usc.edu>")
  # Get the system processor architecture
  message(STATUS "System Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
  # Check if the architecture is x86_64 and set to amd64 for Debian packaging
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
  elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
  else()
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
  endif()

  message(STATUS
    "Debian package architecture: ${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
  # Run 'ldd --version' to get glibc version
  execute_process(
    COMMAND sh -c "ldd --version | head -1 | awk '{print \$NF}'"
    OUTPUT_VARIABLE GLIBC_VERSION
    ERROR_VARIABLE LDD_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(LDD_ERROR OR NOT GLIBC_VERSION)
    message(FATAL_ERROR "Failed to detect glibc version")
  else()
    message(STATUS "Detected glibc version: ${GLIBC_VERSION}")
  endif()
  # Set dependency libc6 with the version constraint
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= ${GLIBC_VERSION})")
endif()

# For Source (only package source from a clean clone)
set(CPACK_SOURCE_IGNORE_FILES build .git .github)
set(CPACK_SOURCE_GENERATOR "TGZ")

include(CPack)
