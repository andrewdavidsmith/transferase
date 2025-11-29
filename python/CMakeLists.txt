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

# Set the Python package/library name
set(LIBRARY_NAME "pyxfr")

message(STATUS "Configuring to build the Python package 'pyxfr'")

# ADS: Steps related to nanobind need to happen in a specific order. I just
# can't remember the details and I don't think I wrote them anywhere...

# For nanobind; seems to work with ordinary 'Development' but
# Development.Module is recommended
set(DEV_MODULE Development.Module)

# Python has to be available and found before nanobind
find_package(Python COMPONENTS Interpreter ${DEV_MODULE} REQUIRED)
message(STATUS "Python include dir: ${Python_INCLUDE_DIRS}")
message(STATUS "Python libraries: ${Python_LIBRARIES}")
message(STATUS "Python version: ${Python_VERSION}")
message(STATUS "Python SOABI: ${Python_SOABI}")

# Check that nanobind is installed by directly calling it
execute_process(
  COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE
  OUTPUT_VARIABLE nanobind_ROOT
  ERROR_QUIET
)
if (NOT nanobind_ROOT)
  message(FATAL_ERROR "Failed to find nanobind (install with pip in .venv)")
endif()

find_package(nanobind CONFIG)

# Required Python packages for the build
set(REQ_PY_PKGS
  nanobind
  pytest
  numpy
  wheel
  hatch
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND REQ_PY_PKGS "auditwheel")
endif()

# Check that each required Python package has been installed
foreach(package_name ${REQ_PY_PKGS})
  message(STATUS "Checking Python package: ${package_name}")
  execute_process(
    COMMAND pip show ${package_name}
    RESULT_VARIABLE EXIT_CODE
    OUTPUT_QUIET
  )
  if(${EXIT_CODE})
    message(FATAL_ERROR "Required Python package not found: ${package_name}")
  endif()
endforeach()

# Configure nanobind
find_package(nanobind CONFIG REQUIRED)

# Setup the Python include and lib directories
include_directories(${Python_INCLUDE_DIRS})
link_directories(${Python_LIBRARIES})

# For release, strip any directories on header filenames
foreach(dirname ${Python_INCLUDE_DIRS})
  list(APPEND STRIP_SUB_LIST "${dirname}/=/")
endforeach()

# Recurse into the subdirectory with the Python bindings
add_subdirectory(pyxfr)

# Configure the tests if requested
if(PYTHON_TESTS)
  # If we are here, we need to setup testing.
  enable_testing()
  find_package(GTest REQUIRED)
  include(GoogleTest)
  add_subdirectory(test)
endif()

## Install the system configuration file
# This might usually only be part of the installation or packageing
# target, but it's needed to run the package and the tests, so it's
# incuded for any build config.
set(PYTHON_DATA_DIR ${PROJECT_BINARY_DIR}/python/${LIBRARY_NAME}/data)
configure_file(${CMAKE_SOURCE_DIR}/data/transferase_data.json
  ${PYTHON_DATA_DIR}/transferase_data_${PROJECT_VERSION}.json
  COPYONLY
)

if(PACKAGE_PYTHON)
  # List of Python packages needed to make the wheel
  set(NEEDED_FOR_PACKAGING wheel hatch)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # On Linux use auditwheel to get the tags postbuild
    list(APPEND NEEDED_FOR_PACKAGING auditwheel)
  endif()

  # Make sure the required Python packages are available; using
  # python3 below because that's available more places than python.
  foreach(package_name ${NEEDED_FOR_PACKAGING})
    execute_process(
      COMMAND python3 -c "import ${package_name}"
      RESULT_VARIABLE PACKAGE_NOT_FOUND
      ERROR_QUIET
    )
    if (PACKAGE_NOT_FOUND)
      message(FATAL_ERROR "Required Python package not found: ${package_name}")
    endif()
  endforeach()

  set(PY_COPY_FILES pyproject.toml hatch_build.py README.md)
  foreach(FILENAME ${PY_COPY_FILES})
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}
      ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME} COPYONLY)
  endforeach()

  # Copy the LICENSE file from the project data dir to the Python build dir
  configure_file(${PROJECT_SOURCE_DIR}/data/LICENSE
    ${CMAKE_CURRENT_BINARY_DIR}/LICENSE COPYONLY)

  # Configure the 'version.py' because it needs 'VERSION' value
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.py
    ${CMAKE_CURRENT_BINARY_DIR}/version.py)

  # Copy the python sources (e.g., "__init__.py") to the directory where they
  # will be included in the package
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pyxfr_utils.py
    ${CMAKE_CURRENT_BINARY_DIR}/${LIBRARY_NAME}/pyxfr_utils.py COPYONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/__init__.py
    ${CMAKE_CURRENT_BINARY_DIR}/${LIBRARY_NAME}/__init__.py COPYONLY)

  # Create a custom target that runs after the build
  add_custom_target(PYTHON_BUILD_WHL ALL
    COMMAND hatch build -t wheel
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${LIBRARY_NAME}
    COMMENT "Using 'hatch' to build the Python package in the build tree"
  )
  execute_process(COMMAND
    python3 ${CMAKE_CURRENT_SOURCE_DIR}/get_python_tag.py
    OUTPUT_VARIABLE PY_VER_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(COMMAND
    python3 ${CMAKE_CURRENT_SOURCE_DIR}/get_abi_tag.py
    OUTPUT_VARIABLE PY_ABI_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  message(STATUS "Platform version tag: ${PY_VER_TAG}")
  message(STATUS "Python ABI tag: ${PY_ABI_TAG}")
  message(STATUS "Python platform tag: determined post-build")

  # The initial wheel filename is determined by the entry in the
  # custom hatch hook that can be found following pyproject.conf
  set(INIT_WHL_FILE "${LIBRARY_NAME}-${PROJECT_VERSION}-py3-none-any.whl")
  message(STATUS "Initial wheel filename: ${INIT_WHL_FILE}")

  set(PLAT_TAG_SCR ${CMAKE_CURRENT_SOURCE_DIR}/get_platform_tag.py)
  set(SHRLIB_DIR ${CMAKE_CURRENT_BINARY_DIR}/${LIBRARY_NAME})

  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # Script to get platform tags
    set(PLAT_TAG_SCR ${CMAKE_CURRENT_SOURCE_DIR}/get_platform_tag.py)
    # Rename the wheel file after it has been generated
    add_custom_target(RENAME_WHL ALL
      COMMAND
      wheel tags
      --python-tag ${PY_VER_TAG}
      --abi-tag ${PY_ABI_TAG}
      --platform-tag `python3 ${PLAT_TAG_SCR} ${SHRLIB_DIR}/${LIBRARY_NAME}*.so`
      ${INIT_WHL_FILE}
      COMMAND rm ${INIT_WHL_FILE}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/dist
      DEPENDS PYTHON_BUILD_WHL
      COMMENT "Adding correct tags to wheel filename (${CMAKE_SYSTEM_NAME})"
    )
  else()
    # Rename the wheel file after it has been generated
    add_custom_target(RENAME_WHL ALL
      COMMAND
      wheel tags
      --python-tag ${PY_VER_TAG}
      --abi-tag ${PY_ABI_TAG}
      --platform-tag `python3 ${PLAT_TAG_SCR} ${INIT_WHL_FILE}`
      ${INIT_WHL_FILE}
      COMMAND rm ${INIT_WHL_FILE}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/dist
      DEPENDS PYTHON_BUILD_WHL
      COMMENT "Adding correct tags to wheel filename (${CMAKE_SYSTEM_NAME})"
    )
  endif()
endif()
