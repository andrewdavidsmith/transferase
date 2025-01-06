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

# Find Python 3 and its necessary components
message(STATUS "Configuring to build Python API")
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
message(STATUS "Python3 include dir: ${Python3_INCLUDE_DIRS}")
message(STATUS "Python3 libraries: ${Python3_LIBRARIES}")
message(STATUS "Python3 version: ${Python3_VERSION}")
find_package(pybind11 REQUIRED)

# Setup the Python include and lib directories
include_directories(${Python3_INCLUDE_DIRS})
link_directories(${Python3_LIBRARIES})

# Strip any directories on header filenames
foreach(dirname ${Python3_INCLUDE_DIRS})
  set(CMAKE_CXX_FLAGS_RELEASE "-ffile-prefix-map=${dirname}/=/ ${CMAKE_CXX_FLAGS}")
  message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
endforeach()

# Recurse into the subdirectory with the Python bindings
add_subdirectory(transferase)

# Set the Python package and library name
set(PYTHON_PACKAGE_NAME "transferase")
set(LIBRARY_NAME "transferase")

# The bindings source at this level needs to see the project headers
include_directories(${PROJECT_SOURCE_DIR}/src)

# Make the package shared library by applying the bindings functions
pybind11_add_module(py_${LIBRARY_NAME} MODULE transferase_bindings.cpp)
target_include_directories(py_${LIBRARY_NAME} PRIVATE transferase)
target_link_libraries(py_${LIBRARY_NAME} PRIVATE
  transferase_bindings
  transferase_system
  pybind11::module
  ZLIB::ZLIB
)
set_target_properties(py_${LIBRARY_NAME}
  PROPERTIES
  LIBRARY_OUTPUT_NAME "${LIBRARY_NAME}"
  SKIP_BUILD_RPATH TRUE
)

# Configure the tests if requested
if(ENABLE_PYTHON_UNIT_TESTS)
  add_subdirectory(test)
endif()
