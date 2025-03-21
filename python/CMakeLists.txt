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

message(STATUS "Configuring to build Python API")

## Try to find nanobind on the system first
# For nanobind before Python; seems to work with ordinary
# 'Development' but Development.Module is recommended
set(DEV_MODULE Development.Module)

# Python has to be available before nanobind
find_package(Python COMPONENTS Interpreter ${DEV_MODULE} REQUIRED)
message(STATUS "Python version: ${Python_VERSION}")
message(STATUS "Python include dir: ${Python_INCLUDE_DIRS}")
message(STATUS "Python libraries: ${Python_LIBRARIES}")
message(STATUS "Python SOABI: ${Python_SOABI}")

# Check that required Python packages have been installed
set(REQUIRED_PYTHON_PACKAGES "nanobind;pytest;wheel;auditwheel;hatch")
foreach(package_name ${REQUIRED_PYTHON_PACKAGES})
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

find_package(nanobind CONFIG REQUIRED)

# Setup the Python include and lib directories
include_directories(${Python_INCLUDE_DIRS})
link_directories(${Python_LIBRARIES})

# For release, strip any directories on header filenames
foreach(dirname ${Python_INCLUDE_DIRS})
  list(APPEND STRIP_SUB_LIST "${dirname}/=/")
endforeach()

# Set the Python package/library name
set(LIBRARY_NAME "transferase")

# Recurse into the subdirectory with the Python bindings
add_subdirectory(transferase)

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
  set(needed_for_packaging wheel hatch)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # On Linux use auditwheel to get the tags postbuild
    list(APPEND needed_for_packaging auditwheel)
  endif()

  # Make sure the required Python packages are available; using
  # python3 below because that's available more places than python.
  foreach(package_name ${needed_for_packaging})
    execute_process(
      COMMAND python3 -c "import ${package_name}"
      RESULT_VARIABLE failed_to_find_package
      OUTPUT_QUIET
      ERROR_QUIET
    )
    if (failed_to_find_package)
      message(FATAL_ERROR "Required Python package not found: ${package_name}")
    endif()
  endforeach()

  ## Configuration for distributing is mutually independent with tests
  # Copy the python/pyproject.toml to the build directory
  configure_file(${PROJECT_SOURCE_DIR}/python/pyproject.toml
    ${PROJECT_BINARY_DIR}/python/pyproject.toml COPYONLY)
  # Copy the hatch_build.py script to the build directory so that the
  # custom build hook can be used by 'hatch build'
  configure_file(${PROJECT_SOURCE_DIR}/python/hatch_build.py
    ${PROJECT_BINARY_DIR}/python/hatch_build.py COPYONLY)
  # Copy the python/README.md to the build directory
  configure_file(${PROJECT_SOURCE_DIR}/python/README.md
    ${PROJECT_BINARY_DIR}/python/README.md COPYONLY)
  configure_file(${PROJECT_SOURCE_DIR}/python/version.py
    ${PROJECT_BINARY_DIR}/python/version.py)
  # Copy the LICENSE file to the build directory
  configure_file(${PROJECT_SOURCE_DIR}/data/LICENSE
    ${PROJECT_BINARY_DIR}/python/LICENSE COPYONLY)

  # Copy the python sources (e.g., "__init__.py") to the directory where they
  # will be included in the package
  configure_file(${PROJECT_SOURCE_DIR}/python/transferase_utils.py
    ${PROJECT_BINARY_DIR}/python/${LIBRARY_NAME}/transferase_utils.py COPYONLY)
  configure_file(${PROJECT_SOURCE_DIR}/python/__init__.py
    ${PROJECT_BINARY_DIR}/python/${LIBRARY_NAME}/__init__.py COPYONLY)

  # Create a custom target that runs after the build
  add_custom_target(PYTHON_BUILD_WHL ALL
    COMMAND hatch build -t wheel
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/python
    DEPENDS ${LIBRARY_NAME}
    COMMENT "Using 'hatch' to build the Python package in the build tree"
  )
  execute_process(COMMAND
    python3 ${PROJECT_SOURCE_DIR}/python/get_python_tag.py
    OUTPUT_VARIABLE PYTHON_VERSION_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(COMMAND
    python3 ${PROJECT_SOURCE_DIR}/python/get_abi_tag.py
    OUTPUT_VARIABLE PYTHON_ABI_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  message(STATUS "Platform version tag: ${PYTHON_VERSION_TAG}")
  message(STATUS "Python ABI tag: ${PYTHON_ABI_TAG}")
  message(STATUS "Python platform tag: determined post-build")

  # The initial wheel filename is determined by the entry in the
  # custom hatch hook that can be found following pyproject.conf
  set(INIT_WHL_FILE "${LIBRARY_NAME}-${PROJECT_VERSION}-py3-none-any.whl")
  message(STATUS "Initial wheel filename: ${INIT_WHL_FILE}")

  # Rename the wheel file after it has been generated
  add_custom_target(PYTHON_RENAME_WHL ALL
    COMMAND
    wheel tags
    --python-tag ${PYTHON_VERSION_TAG}
    --abi-tag ${PYTHON_ABI_TAG}
    --platform-tag
    `python3 ${PROJECT_SOURCE_DIR}/python/get_platform_tag.py ${INIT_WHL_FILE}`
    ${INIT_WHL_FILE}
    COMMAND rm ${INIT_WHL_FILE}
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/python/dist
    DEPENDS PYTHON_BUILD_WHL
    COMMENT "Adding correct tags to wheel filename"
  )
endif()
