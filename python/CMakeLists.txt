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

# Setup the Python include and lib directories
include_directories(${Python3_INCLUDE_DIRS})
link_directories(${Python3_LIBRARIES})

# For release, strip any directories on header filenames
foreach(dirname ${Python3_INCLUDE_DIRS})
  list(APPEND STRIP_SUB_LIST "${dirname}/=/")
endforeach()

# Set the Python package/library name
set(LIBRARY_NAME "transferase")

# Recurse into the subdirectory with the Python bindings
add_subdirectory(transferase)

# Configure the tests if requested
if(ENABLE_PYTHON_UNIT_TESTS)
  add_subdirectory(test)
endif()

# Copy the python/pyproject.toml to the build directory
configure_file(${PROJECT_SOURCE_DIR}/python/pyproject.toml
               ${PROJECT_BINARY_DIR}/python/pyproject.toml COPYONLY)
# Copy the python/README.md to the build directory
configure_file(${PROJECT_SOURCE_DIR}/python/README.md
               ${PROJECT_BINARY_DIR}/python/README.md COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/python/version.py
               ${PROJECT_BINARY_DIR}/python/version.py)
# Copy the LICENSE file to the build directory
configure_file(${PROJECT_SOURCE_DIR}/LICENSE
               ${PROJECT_BINARY_DIR}/python/LICENSE COPYONLY)
file(TOUCH ${PROJECT_BINARY_DIR}/python/${LIBRARY_NAME}/__init__.py)

# Create a custom target that runs after the build
add_custom_target(PYTHON_BUILD_WHL ALL
  COMMAND hatch build
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/python
  DEPENDS py_${LIBRARY_NAME}
  COMMENT "Use hatch to build the Python package inside the build tree"
)

# Rename the wheel file after it has been generated
add_custom_target(PYTHON_RENAME_WHL ALL
  COMMAND mv ${LIBRARY_NAME}-*.whl ${LIBRARY_NAME}.${Python3_SOABI}.whl
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/python/dist
  DEPENDS PYTHON_BUILD_WHL
  COMMENT "Rename the wheel hatch built so it has correct tags in the filename"
)
