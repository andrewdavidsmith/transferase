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

# Set static for 3rd party libraries -- that aren't header-only

message(STATUS "Enabling static linkage for all non-system libraries")

# required
set(ZLIB_USE_STATIC_LIBS on)

# Python3 and nanobind: not relevant here

# optional
set(Curses_USE_STATIC_LIBS on)  # cmake-lint: disable=C0103

# Set static for the linker so the compiler's libraries will be static
## ADS: using this instead of forcing -static for everything avoids the static
## linkage that Aiso warns against, but also means it's not 100% static linked
list(APPEND GLOBAL_LINKER_OPTIONS -static-libgcc -static-libstdc++)
