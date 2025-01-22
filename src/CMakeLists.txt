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

file(GLOB cpp_files "*.cpp")
list(FILTER cpp_files EXCLUDE REGEX "transferase.cpp")

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

## Passing BOOST_JSON_NO_LIB to the boost_json library will turn on
## header-only mode, which is required for -fPIC, which is required
## for building the Python library.
if(BOOST_JSON_NO_LIB)
  target_compile_definitions(boost_json PRIVATE BOOST_JSON_NO_LIB)
endif()

## Make sure that the command_select.cpp source can see the header for
## ncurses if the ncurses library was found
if(CURSES_FOUND)
  target_include_directories(command_select PRIVATE ${CURSES_INCLUDE_DIRS})
endif()

# Create the initial static library linking the individual objects
add_library(transferase_system STATIC)
target_link_options(transferase_system PRIVATE ${GLOBAL_LINKER_OPTIONS})
target_link_libraries(transferase_system PRIVATE ${LIBRARY_OBJECTS})

# Create the executable
add_executable(transferase transferase.cpp)
# Make sure that the 'config.h' is visible to include
target_include_directories(transferase PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries(transferase PRIVATE
  transferase_system
  ZLIB::ZLIB
  ${Boost_LIBRARIES}
  Threads::Threads
)
if(CURSES_FOUND)
  target_link_libraries(transferase PRIVATE ${CURSES_LIBRARIES})
endif()

target_link_options(transferase PRIVATE ${GLOBAL_LINKER_OPTIONS})
target_compile_options(transferase PRIVATE ${GLOBAL_COMPILE_OPTIONS})
