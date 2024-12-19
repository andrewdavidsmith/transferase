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
project(
  transferase
  VERSION 0.2.0
  DESCRIPTION "The transferase system for retrieving methylomes"
  HOMEPAGE_URL https://github.com/andrewdavidsmith/transferase
  LANGUAGES CXX)

# Set language version used
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSIONS off)  # prevents std=gnu++23
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

include(CheckIncludeFileCXX)
include(CheckFunctionExists)

# Options
option(ENABLE_UNIT_TESTS "Enable unit tests" off)
option(ENABLE_STATIC_ANALYSIS "Enable static analysis" off)
option(USE_STATIC_LIBS "Use static linkage for non-system libs" off)
option(ENABLE_SANITIZER "Enable sanitizer" off)
option(SANITIZER_TYPE "Choose sanitizer type (address, undefined)" "address")

# Sanitizers
if(ENABLE_SANITIZER)
  if(SANITIZER_TYPE STREQUAL "address")
    set(SANITIZER_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
  elseif(SANITIZER_TYPE STREQUAL "undefined")
    set(SANITIZER_FLAGS "-fsanitize=undefined -fno-omit-frame-pointer")
  else()
    # ADS: others don't seem to work...
    message(FATAL_ERROR "Unsupported sanitizer type: ${SANITIZER_TYPE}")
  endif()
  # Set flags for the compiler and linker
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
  set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
  message(STATUS "Sanitizer enabled: ${SANITIZER_TYPE}")
endif()

# Static analysis
if(ENABLE_STATIC_ANALYSIS)
  message(STATUS "Static analysis enabled")

  # cpplint: all options are in the config file
  find_program(FOUND_CPPLINT cpplint)
  if(FOUND_CPPLINT)
    message(STATUS "Enabling cpplint analysis")
    set(CMAKE_CXX_CPPLINT
      cpplint
      --quiet
    )
  endif()

  find_program(FOUND_IWYU include-what-you-use)
  if(FOUND_IWYU)
    message(STATUS "Enabling include-what-you-use analysis")
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE
      include-what-you-use
      -Xiwyu
      --mapping_file=${PROJECT_SOURCE_DIR}/iwyu.json
    )
  endif()

  # cppcheck: options on the command line as there is no config file
  find_program(FOUND_CPPCHECK cppcheck)
  if(FOUND_CPPCHECK)
    message(STATUS "Enabling cppcheck analysis")
    set(CMAKE_CXX_CPPCHECK
      cppcheck
      --quiet
      --enable=all
      --check-level=exhaustive
      --suppressions-list=${PROJECT_SOURCE_DIR}/.cppcheck_suppress
    )
  endif()
  # clang-tidy: currently needs at least v20.0
  # set(CMAKE_CXX_CLANG_TIDY
  #   clang-tidy
  #   --quiet
  #   -p Build
  # )
endif()

# Set these before looking for the corresponding libraries
if(USE_STATIC_LIBS)
  message(STATUS "Static linkage for non-system libs")
  set(Boost_USE_STATIC_LIBS on)
  set(ZLIB_USE_STATIC_LIBS on)
  # Replace any home dir prefix in paths
  set(HOME_DIR $ENV{HOME}) # For Unix-like systems
  set(PREFIX_MAP "${HOME_DIR}/=/")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffile-prefix-map=${PREFIX_MAP}")
  # Boost asserts can leave paths; disable them
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_DISABLE_ASSERTS")
  # Make sure debugging is turned off and strip symbols
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s -g0")
endif()

# Find dependencies
find_package(Threads REQUIRED)
find_package(
  Boost
  1.85.0 REQUIRED
  COMPONENTS
  program_options json context)
find_package(ZLIB REQUIRED)

if(ENABLE_UNIT_TESTS)
  message(STATUS "Unit tests enabled")
  add_compile_definitions(UNIT_TEST)
  enable_testing()
  add_subdirectory(test)
endif()

# Add the main source directory before we add the test subdir below
add_subdirectory(src)

# Make config.h
include(GNUInstallDirs)
set(DATADIR "${CMAKE_INSTALL_DATADIR}")
message(STATUS "Relative data directory: ${DATADIR}")
configure_file(config.h.in config.h)

# Make the needed dir in build path and copy the versioned system
# config data there (ADS: this is way too confusing). This is needed
# for running unit tests and for running 'xfrase config' generally
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${DATADIR}/transferase)
configure_file(${CMAKE_SOURCE_DIR}/data/transferase_data.json
  ${CMAKE_BINARY_DIR}/${DATADIR}/transferase/transferase_data_${PROJECT_VERSION}.json
  COPYONLY
)

if (NOT ENABLE_UNIT_TESTS)
  # Installation
  # cmake --install build_dir --prefix /path/to/install/root
  install(TARGETS xfrase
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
  # Install an alias with the full project name
  install(PROGRAMS $<TARGET_FILE:xfrase>
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    RENAME transferase
  )
  # Install the versioned JSON file to the appropriate share directory
  # (e.g., /usr/local/share/transferase)
  install(FILES ${CMAKE_BINARY_DIR}/share/transferase/transferase_data_${PROJECT_VERSION}.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/transferase
  )
  # Install the bash completions script to the appropriate directory
  # (e.g., /usr/share/bash-completion/completions/)
  install(FILES ${CMAKE_SOURCE_DIR}/data/xfrase-bash-completion
    DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
    RENAME xfrase
  )

  ### Packaging
  set(CPACK_STRIP_FILES ON)
  set(CPACK_PACKAGE_VENDOR "Andrew D Smith")
  set(CPACK_GENERATOR "TGZ;STGZ;DEB")
  set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE)
  set(CPACK_PACKAGE_CONTACT "Andrew D Smith")

  ## For STGZ
  set(CPACK_STGZ_HEADER_FILE "${CMAKE_SOURCE_DIR}/modules/CPack.STGZ_Header.sh.in")

  ## For Debian
  # Get the system processor architecture
  message(STATUS "System Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
  # Check if the architecture is x86_64 and set to amd64 for Debian packaging
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
  elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
  else()
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
  endif()

  # Print the architecture being used for the Debian package
  message(STATUS "Debian Package Architecture: ${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Andrew D Smith <andrewds@usc.edu>")
  # Run `ldd --version` to get glibc version
  execute_process(
    COMMAND sh -c "ldd --version | head -1 | awk '{print \$NF}'"
    OUTPUT_VARIABLE GLIBC_VERSION
    ERROR_VARIABLE LDD_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(GLIBC_VERSION AND NOT LDD_ERROR)
    message(STATUS "Detected glibc version: ${GLIBC_VERSION}")
  else()
    message(FATAL_ERROR "Failed to detect glibc version")
  endif()
  # Set dependency libc6 with the version constraint
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= ${GLIBC_VERSION})")

  ### Only package source from a clean clone
  set(CPACK_SOURCE_IGNORE_FILES build .git .github)
  set(CPACK_SOURCE_GENERATOR "TGZ")
  include(CPack)
endif()
