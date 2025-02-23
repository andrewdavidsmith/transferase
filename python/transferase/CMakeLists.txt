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

nanobind_build_library(nanobind-static-abi3)

set(NANOBIND_COMPILE_OPTIONS ${GLOBAL_COMPILE_OPTIONS})
list(REMOVE_ITEM NANOBIND_COMPILE_OPTIONS "-Wpedantic")

file(GLOB cpp_files "*.cpp")
list(FILTER cpp_files EXCLUDE REGEX "transferase_bindings.cpp")

# Ensure all objects depend on both the cpp and hpp files
set(LIBRARY_OBJECTS "")
foreach(cpp_file ${cpp_files})
  get_filename_component(BASE_NAME ${cpp_file} NAME_WE)
  add_library(${BASE_NAME} OBJECT ${cpp_file})
  target_link_libraries(${BASE_NAME} PUBLIC
    nanobind-static-abi3
    transferase_system
  )
  target_compile_options(${BASE_NAME} PRIVATE
    ${NANOBIND_COMPILE_OPTIONS}
  )
  list(APPEND LIBRARY_OBJECTS ${BASE_NAME})
endforeach()

# Create the static library by linking the previous parts
add_library(transferase_bindings STATIC transferase_bindings.cpp)
target_link_libraries(transferase_bindings PUBLIC ${LIBRARY_OBJECTS})

# Make the package shared library by applying the bindings functions
nanobind_add_module(
  ${LIBRARY_NAME}
  NB_STATIC STABLE_ABI LTO
  transferase_bindings.cpp
)

# Compile the extension library
target_link_libraries(${LIBRARY_NAME} PUBLIC transferase_bindings)

target_compile_options(
  ${LIBRARY_NAME}
  PRIVATE
  ${GLOBAL_COMPILE_OPTIONS_FOR_NANOBIND}
)

set(GLOBAL_LINKER_OPTIONS_FOR_NANOBIND ${GLOBAL_LINKER_OPTIONS})
target_link_options(
 ${LIBRARY_NAME}
 PRIVATE
 ${GLOBAL_LINKER_OPTIONS_FOR_NANOBIND}
)

set_target_properties(${LIBRARY_NAME}
  PROPERTIES
  LIBRARY_OUTPUT_NAME ${LIBRARY_NAME}
  SKIP_BUILD_RPATH TRUE
)
