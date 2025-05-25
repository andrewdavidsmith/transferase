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

find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

find_package(Threads REQUIRED)

message(STATUS "Adding external header-only project 'asio'")
include(FetchContent)
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/andrewdavidsmith/asio.git
  GIT_TAG main
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  SYSTEM
  OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(asio)
FetchContent_GetProperties(asio SOURCE_DIR ASIO_SOURCE_DIR)

file(GLOB cpp_files "*.cpp")

# Gather all the object files that will be put in the static library
# and prepare to compile them.
set(LIBRARY_OBJECTS "")
foreach(cpp_file ${cpp_files})
  get_filename_component(BASE_NAME ${cpp_file} NAME_WE)
  add_library(${BASE_NAME} OBJECT ${cpp_file})
  ## Below is to make sure 'config.h' is visible for includes and any
  ## of the headers for configured libraries
  target_include_directories(${BASE_NAME} PUBLIC
    ${PROJECT_BINARY_DIR}
    ${ZLIB_INCLUDE_DIRS}
    ${ASIO_SOURCE_DIR}
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
  OpenSSL::SSL
  OpenSSL::Crypto
)

if(TEST_LIB)
  add_subdirectory(tests)
endif()
