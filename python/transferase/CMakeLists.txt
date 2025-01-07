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

# ADS: Not sure how many times this is getting included
include_directories(${PROJECT_SOURCE_DIR}/src)

file(GLOB CPP_FILES "*.cpp")

# Ensure all objects depend on both the cpp and hpp files
set(LIBRARY_OBJECTS)
foreach(CPP_FILE ${CPP_FILES})
  get_filename_component(BASE_NAME ${CPP_FILE} NAME_WE)
  set(HPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${BASE_NAME}.hpp")
  set(HPP_FILE "${BASE_NAME}.hpp")
  if(EXISTS ${HPP_PATH})
    add_library(${BASE_NAME} OBJECT ${CPP_FILE} ${HPP_FILE})
    target_include_directories(${BASE_NAME} PRIVATE ${PROJECT_BINARY_DIR})
    target_link_libraries(${BASE_NAME} PRIVATE pybind11::module)
    list(APPEND LIBRARY_OBJECTS ${BASE_NAME})
  endif()
endforeach()

# Create the final static library by linking the previous parts
add_library(transferase_bindings STATIC)
target_link_libraries(transferase_bindings PRIVATE ${LIBRARY_OBJECTS})

# Make the package shared library by applying the bindings functions
pybind11_add_module(py_${LIBRARY_NAME} MODULE transferase_bindings.cpp)
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
