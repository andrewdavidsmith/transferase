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

# Remove things that will be handled later
list(FILTER cpp_files EXCLUDE REGEX "transferase.cpp")
# list(FILTER cpp_files EXCLUDE REGEX "command_select.cpp")

# Gather all the object files that will be put in the static library
# and prepare to compile them.
set(LIBRARY_OBJECTS "")
foreach(cpp_file ${cpp_files})
  get_filename_component(BASE_NAME ${cpp_file} NAME_WE)
  add_library(${BASE_NAME} OBJECT ${cpp_file})
  # Below: make sure 'config.h' is visible
  target_include_directories(${BASE_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/lib)
  target_compile_options(${BASE_NAME} PRIVATE ${GLOBAL_COMPILE_OPTIONS})
  target_link_libraries(${BASE_NAME} PRIVATE transferase_system)
  list(APPEND LIBRARY_OBJECTS ${BASE_NAME})
endforeach()

# Curses
include(FindCurses)  # This should be in the modules dir
set(CURSES_NEED_NCURSES true)
find_package(Curses)
if(CURSES_FOUND AND CURSES_HAVE_NCURSES_H)
  # ADS: Currently the ncurses library on the build development needs
  # to link with the GPM library; this is not optimal, but also not a
  # priority now.
  set(GPM_LIB_NAME gpm)
  if (USE_STATIC_LIBS)
    set(GPM_LIB_NAME libgpm.a)
  endif()
  find_library(GPM_LIB ${GPM_LIB_NAME} NO_CACHE)
  if (GPM_LIB STREQUAL GPM_LIB-NOTFOUND)
    message(STATUS "Failed to find GPM lib: ${GPM_LIB_NAME}")
  else()
    message(STATUS "Found GPM library: ${GPM_LIB_NAME} (check if correct)")
    list(APPEND CURSES_LIBRARIES ${GPM_LIB})
  endif()
  # ADS: need <ncurses.h> and not <curses.h> or <ncurses/curses.h>
  message(STATUS "Found ncurses header: ${CURSES_HAVE_NCURSES_H}")
  message(STATUS "Found ncurses libraries: ${CURSES_LIBRARIES}")
  target_compile_definitions(command_select PRIVATE HAVE_NCURSES)
  target_include_directories(command_select PRIVATE ${CURSES_INCLUDE_DIRS})
else()
  message(STATUS "NCurses not found: 'select' will be disabled")
endif()

# Create the initial static library linking the individual objects
add_library(transferase_cli STATIC)
target_include_directories(transferase_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_options(transferase_cli PRIVATE ${GLOBAL_LINKER_OPTIONS})
# Specify all libraries here. In case of dynamic linking they need to
# have the proper order on the compiler command
target_link_libraries(transferase_cli PUBLIC ${LIBRARY_OBJECTS})

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
  target_link_libraries(${name} PRIVATE transferase_system transferase_cli)
  if(CURSES_FOUND)
    target_link_libraries(${name} PRIVATE ${CURSES_LIBRARIES})
  endif()
  target_link_options(${name} PRIVATE ${GLOBAL_LINKER_OPTIONS})
  target_compile_options(${name} PRIVATE ${GLOBAL_COMPILE_OPTIONS})
endforeach()

if(TEST_CLI)
  # The UNIT_TEST preprocessor variable turns off the static and inline
  # on several functions so they can be tested; all sources could need this.
  add_compile_definitions(UNIT_TEST)
  enable_testing()
  find_package(GTest REQUIRED)
  include(GoogleTest)
  add_subdirectory(tests)
endif()
