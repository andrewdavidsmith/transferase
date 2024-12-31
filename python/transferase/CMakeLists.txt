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

# Add bindings files
set(BINDINGS_OBJECTS
  cpg_index_metadata
  query
)

# Add bindings source files as targets
foreach(OBJ ${BINDINGS_OBJECTS})
  # Create the pybind11 module
  pybind11_add_module(py_${OBJ} MODULE ${OBJ}_bindings.cpp)
  target_link_libraries(py_${OBJ} PRIVATE xfrase_system)
  # Must come after linking to any static libs
  target_link_libraries(py_${OBJ} PRIVATE pybind11::module)
  set_target_properties(py_${OBJ} PROPERTIES LIBRARY_OUTPUT_NAME "${OBJ}")
endforeach()

# ADS: Not sure how many times this is getting included
include_directories(${PROJECT_SOURCE_DIR}/src)
