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

file(GLOB cpp_files "*.cpp")
list(FILTER cpp_files EXCLUDE REGEX "transferase_bindings.cpp")

# Ensure all objects depend on both the cpp and hpp files
set(LIBRARY_OBJECTS "")
foreach(cpp_file ${cpp_files})
  get_filename_component(BASE_NAME ${cpp_file} NAME_WE)
  add_library(${BASE_NAME} OBJECT ${cpp_file})
  # Make sure the individual bindings compile jobs can see the
  # transferase headers and also the config.h header in the binary dir
  target_include_directories(${BASE_NAME} PRIVATE
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/src
  )
  # Below, link to the pybind11::module to get the headers; these
  # won't be linked on their own
  target_link_libraries(${BASE_NAME} PRIVATE
    pybind11::module
    ${Boost_LIBRARIES}
  )
  target_compile_options(${BASE_NAME} PRIVATE ${GLOBAL_COMPILE_OPTIONS})
  list(APPEND LIBRARY_OBJECTS ${BASE_NAME})
endforeach()

# Create the final static library by linking the previous parts
add_library(transferase_bindings STATIC)
target_link_libraries(transferase_bindings PRIVATE ${LIBRARY_OBJECTS})

# Make the package shared library by applying the bindings functions;
# use the py_ prefix because otherwise there might be name clash with
# the main 'transferase' target in a different directory.
pybind11_add_module(py_${LIBRARY_NAME} MODULE transferase_bindings.cpp)
# Make sure the compile job for the module can see the transferase
# headers and also the config.h header in the binary dir
target_include_directories(py_${LIBRARY_NAME} PRIVATE
  ${PROJECT_BINARY_DIR}
  ${PROJECT_SOURCE_DIR}/src
)

# ADS: below, need the Boost_LIBRARIES because otherwise problems
# arise with undefined symols for jump_fcontext, ontop_fcontext and
# others related to boost context at runtime within the Python
# interpreter. Not sure it makes sense. I think it's related to the
# fact that the header from
target_link_libraries(py_${LIBRARY_NAME} PRIVATE
  transferase_bindings
  transferase_system
  pybind11::module
  ZLIB::ZLIB
)
target_compile_options(py_${LIBRARY_NAME} PRIVATE ${GLOBAL_COMPILE_OPTIONS})
target_link_options(py_${LIBRARY_NAME} PRIVATE ${GLOBAL_LINKER_OPTIONS})

set_target_properties(py_${LIBRARY_NAME}
  PROPERTIES
  LIBRARY_OUTPUT_NAME ${LIBRARY_NAME}
  SKIP_BUILD_RPATH TRUE
)
