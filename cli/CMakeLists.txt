# MIT License
#
# Copyright (c) 2025 Andrew Smith
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

# Can't have this among the sources or a name will clash
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
  target_link_libraries(${BASE_NAME} PRIVATE transferase_system)
  target_compile_options(${BASE_NAME} PRIVATE ${GLOBAL_COMPILE_OPTIONS})
  list(APPEND LIBRARY_OBJECTS ${BASE_NAME})
endforeach()

## Make sure that the command_select.cpp source can see the header for
## ncurses if the ncurses library was found
if(CURSES_FOUND)
  target_include_directories(command_select PRIVATE ${CURSES_INCLUDE_DIRS})
endif()

# Create the initial static library linking the individual objects
add_library(transferase_cli STATIC)
target_include_directories(transferase_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_options(transferase_cli PRIVATE ${GLOBAL_LINKER_OPTIONS})
target_link_libraries(transferase_cli PRIVATE ${LIBRARY_OBJECTS})


# ADS: this approach is the most reliable way so far to have the alias
# both installed properly and linked the same way as tranferase.
set(TRANSFERASE_NAMES "transferase")
if(BUILD_XFR_ALIAS)
  list(APPEND TRANSFERASE_NAMES "xfr")
endif()

# Create the executable
foreach(name ${TRANSFERASE_NAMES})
  add_executable(${name} transferase.cpp)
  # Make sure that the 'config.h' is visible to include
  target_include_directories(${name} PRIVATE ${PROJECT_BINARY_DIR})
  target_link_libraries(${name} PRIVATE
    transferase_system
    transferase_cli
    ZLIB::ZLIB
    ${Boost_LIBRARIES}
    Threads::Threads
  )
  if(CURSES_FOUND)
    target_link_libraries(${name} PRIVATE ${CURSES_LIBRARIES})
  endif()
  target_link_options(${name} PRIVATE ${GLOBAL_LINKER_OPTIONS})
  target_compile_options(${name} PRIVATE ${GLOBAL_COMPILE_OPTIONS})
endforeach()
