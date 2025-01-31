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

# Find Python
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# List of Python packages needed to do the tests
set(needed_for_tests pytest)

# Make sure the required Python packages are available
foreach(package_name ${needed_for_tests})
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

# Get all .py files in the python/test/ directory:
file(GLOB PYTHON_TEST_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*_test.py)

# Specify the additional path for Python to find modules:
set(PYTHONPATH "${PROJECT_BINARY_DIR}/python" ${PYTHONPATH})

# Copy the matched .py files to the build directory and add a test for
# each of them
foreach(TEST_FILE ${PYTHON_TEST_FILES})
  file(COPY ${TEST_FILE} DESTINATION ${PROJECT_BINARY_DIR}/python/test)
  get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
  add_test(
    NAME python_${TEST_NAME}
    COMMAND ${Python3_EXECUTABLE} -m pytest
    ${PROJECT_BINARY_DIR}/python/test
  )
  set_tests_properties(python_${TEST_NAME}
    PROPERTIES ENVIRONMENT
    "PYTHONPATH=${PROJECT_BINARY_DIR}/python/transferase")
endforeach()

# Ensure test data dir is in the right place relative to ctest paths:
file(CREATE_LINK
  ${PROJECT_SOURCE_DIR}/test/data
  ${PROJECT_BINARY_DIR}/python/test/data
  SYMBOLIC
)
