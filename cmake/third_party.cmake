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

message(STATUS "Locating required third-party packages")

find_package(Threads REQUIRED)

set(BOOST_COMPONENTS program_options context)
## Building python lib needs -fPIC and Boost.json has an issue with
## this so BOOST_JSON_NO_LIB will be passed to sources and turn on
## header-only for Boost.json
if(NOT BUILD_PYTHON)
  list(APPEND BOOST_COMPONENTS json)
else()
  set(BOOST_JSON_NO_LIB on)
endif()
find_package(
  Boost
  1.85.0 REQUIRED
  COMPONENTS
  ${BOOST_COMPONENTS}
)

find_package(ZLIB REQUIRED)

message(STATUS "Locating optional third-party packages")

set(CURSES_NEED_NCURSES TRUE)
find_package(Curses)
if(CURSES_FOUND)
  add_compile_definitions(HAVE_NCURSES)
else()
  message(STATUS "NCurses not found; 'select' command will not be built")
endif()
