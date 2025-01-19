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

if(BUILD_PYTHON)
  message(FATAL_ERROR "Sanitizing unsupported when building Python package")
endif()

if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
  message(FATAL_ERROR "Sanitizing only supported for GCC")
endif()

set(SANITIZER_FLAGS "")
list(APPEND SANITIZER_FLAGS -fno-omit-frame-pointer)
if(SANITIZER_TYPE STREQUAL "address")
  list(APPEND SANITIZER_FLAGS -fsanitize=address)
elseif(SANITIZER_TYPE STREQUAL "undefined")
  list(APPEND SANITIZER_FLAGS -fsanitize=undefined)
else()
  # ADS: others don't seem to work yet...
  message(FATAL_ERROR "Unsupported sanitizer type: ${SANITIZER_TYPE}")
endif()

message(STATUS "Sanitizer enabled: ${SANITIZER_TYPE}")
message(STATUS "Sanitizer options: ${SANITIZER_FLAGS}")

# Set flags for the compiler and linker
list(APPEND GLOBAL_COMPILE_OPTIONS ${SANITIZER_FLAGS})
list(APPEND GLOBAL_LINKER_OPTIONS ${SANITIZER_FLAGS})
