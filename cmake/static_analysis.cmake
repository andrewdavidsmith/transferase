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

message(STATUS "Static analysis enabled")

# cpplint: all options are in the config file
find_program(FOUND_CPPLINT cpplint)
if(FOUND_CPPLINT)
  message(STATUS "Enabling cpplint analysis")
  set(CMAKE_CXX_CPPLINT
    cpplint
    --quiet
  )
endif()

find_program(FOUND_IWYU include-what-you-use)
if(FOUND_IWYU)
  message(STATUS "Enabling include-what-you-use analysis")
  set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE
    include-what-you-use
    -Xiwyu
    --comment_style=long
    -Xiwyu
    --quoted_includes_first
    -Xiwyu
    --mapping_file=${PROJECT_SOURCE_DIR}/iwyu.json
  )
endif()

# cppcheck: options on the command line as there is no config file
find_program(FOUND_CPPCHECK cppcheck)
if(FOUND_CPPCHECK)
  message(STATUS "Enabling cppcheck analysis")
  set(CMAKE_CXX_CPPCHECK
    cppcheck
    --quiet
    --enable=all
    --check-level=exhaustive
    --suppressions-list=${PROJECT_SOURCE_DIR}/.cppcheck_suppress
  )
endif()

# clang-tidy: need to make sure version is at least 20
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
# Minimum required version
set(MIN_CLANG_TIDY_VERSION "20.0.0")
if(CLANG_TIDY_EXECUTABLE)
  execute_process(
    COMMAND bash -c "${CLANG_TIDY_EXECUTABLE} --version | grep version | tr -cd '0-9.\n'"
    OUTPUT_VARIABLE CLANG_TIDY_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  # Compare the version numbers
  if(CLANG_TIDY_VERSION VERSION_GREATER_EQUAL MIN_CLANG_TIDY_VERSION)
    message(STATUS "Enabling clang-tidy (version: ${CLANG_TIDY_VERSION})")
    set(CMAKE_CXX_CLANG_TIDY
      clang-tidy
      --quiet
      -p Build
    )
  else()
    message(STATUS "Not enabling clang-tidy (version not found")
  endif()
endif()
