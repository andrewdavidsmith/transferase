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

file(GLOB CPP_FILES "*.cpp")

# Ensure all objects depend on both the cpp and hpp files
set(LIBRARY_OBJECTS)
foreach(CPP_FILE ${CPP_FILES})
  get_filename_component(BASE_NAME ${CPP_FILE} NAME_WE)
  set(HPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${BASE_NAME}.hpp")
  set(HPP_FILE "${BASE_NAME}.hpp")
  if(EXISTS ${HPP_PATH})
    add_library(${BASE_NAME} OBJECT ${CPP_FILE} ${HPP_FILE})
    ## below, make sure config.h is visible to include
    target_include_directories(${BASE_NAME} PRIVATE ${PROJECT_BINARY_DIR})
    list(APPEND LIBRARY_OBJECTS ${BASE_NAME})
  endif()
endforeach()

## Passing BOOST_JSON_NO_LIB to the boost_json library will turn on
## header-only mode, which is required for -fPIC, which is required
## for building the Python library.
if(BOOST_JSON_NO_LIB)
  target_compile_definitions(boost_json PRIVATE BOOST_JSON_NO_LIB)
endif()

# Create the final library by linking the previous parts
add_library(transferase_system STATIC)
target_link_libraries(transferase_system PRIVATE ${LIBRARY_OBJECTS})

add_executable(transferase transferase.cpp)
target_include_directories(transferase PRIVATE "${PROJECT_BINARY_DIR}")
target_link_libraries(transferase PRIVATE
  ${COMMAND_OBJECTS}
  transferase_system
  ZLIB::ZLIB
  ${Boost_LIBRARIES}
  Threads::Threads
)
if(CURSES_FOUND)
  target_link_libraries(transferase PRIVATE
    ${CURSES_LIBRARIES}
  )
endif()
