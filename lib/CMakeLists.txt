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

# Boost components
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()
set(BOOST_COMPONENTS context)
find_package(Boost 1.83.0 REQUIRED COMPONENTS ${BOOST_COMPONENTS})
message(STATUS "Boost version: ${Boost_VERSION_STRING}")
message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost library dirs: ${Boost_LIBRARY_DIRS}")

find_package(Threads REQUIRED)

file(GLOB cpp_files "*.cpp")

# Gather all the object files that will be put in the static library
# and prepare to compile them.
set(LIBRARY_OBJECTS "")
foreach(cpp_file ${cpp_files})
  get_filename_component(BASE_NAME ${cpp_file} NAME_WE)
  add_library(${BASE_NAME} OBJECT ${cpp_file})
  ## Below is to make sure 'config.h' is visible for includes and any
  ## of the headers for configured libraries
  target_include_directories(${BASE_NAME} PRIVATE
    ${PROJECT_BINARY_DIR}
    ${Boost_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
  )
  target_compile_options(${BASE_NAME} PRIVATE ${GLOBAL_COMPILE_OPTIONS})
  list(APPEND LIBRARY_OBJECTS ${BASE_NAME})
endforeach()

# Create the initial static library linking the individual objects
add_library(transferase_system STATIC)
target_include_directories(transferase_system PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_options(transferase_system PRIVATE ${GLOBAL_LINKER_OPTIONS})
target_link_libraries(transferase_system PUBLIC
  ${LIBRARY_OBJECTS}
  ZLIB::ZLIB
  Threads::Threads
  ${Boost_LIBRARIES}
)

if(TEST_LIB)
  # The UNIT_TEST preprocessor variable turns off the static and inline
  # on several functions so they can be tested; all sources could need this.
  add_compile_definitions(UNIT_TEST)
  enable_testing()
  find_package(GTest REQUIRED)
  include(GoogleTest)
  add_subdirectory(tests)
endif()
