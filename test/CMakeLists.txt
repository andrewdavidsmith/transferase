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

find_package(GTest REQUIRED)
include(GoogleTest)

include_directories(${PROJECT_SOURCE_DIR}/src)

add_library(unit_test_utils OBJECT unit_test_utils.cpp unit_test_utils.hpp)

file(GLOB CPP_FILES "*_test.cpp")

# Ensure all objects depend on both the cpp and hpp files
set(TEST_OBJECTS)
foreach(CPP_FILE ${CPP_FILES})
  get_filename_component(BASE_NAME ${CPP_FILE} NAME_WE)
  add_executable(${BASE_NAME} ${CPP_FILE})
  target_link_libraries(${BASE_NAME}
    PRIVATE
    GTest::GTest
    GTest::Main
    ZLIB::ZLIB
    xfrase_system
  )
  target_include_directories(${BASE_NAME} PRIVATE ${PROJECT_BINARY_DIR})
  if(BASE_NAME MATCHES "^command_")
    # Remove the substring from the filename
    string(REPLACE "_test" "" COMMAND_LIBRARY ${BASE_NAME})
    set(SOURCE_FILE "${PROJECT_SOURCE_DIR}/src/${COMMAND_LIBRARY}.cpp")
    if(EXISTS ${SOURCE_FILE})
      target_link_libraries(${BASE_NAME} PRIVATE ${COMMAND_LIBRARY})
    endif()
    target_link_libraries(${BASE_NAME} PRIVATE unit_test_utils)
  endif()
  list(APPEND TEST_OBJECTS ${BASE_NAME})
endforeach()

# Define the UNIT_TEST macro for the test targets
foreach(target ${TEST_OBJECTS})
  target_compile_definitions(${target} PRIVATE UNIT_TEST)
endforeach()

foreach(target ${TEST_OBJECTS})
  gtest_discover_tests(${target})
endforeach()

# Ensure test data dir is in the right place relative to ctest paths
file(CREATE_LINK
  ${PROJECT_SOURCE_DIR}/test/data
  ${PROJECT_BINARY_DIR}/test/data
  SYMBOLIC
)
