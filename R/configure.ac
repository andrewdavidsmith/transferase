dnl MIT License
dnl
dnl Copyright (c) 2025 Andrew Smith
dnl
dnl Permission is hereby granted, free of charge, to any person obtaining a copy
dnl of this software and associated documentation files (the "Software"), to deal
dnl in the Software without restriction, including without limitation the rights
dnl to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
dnl copies of the Software, and to permit persons to whom the Software is
dnl furnished to do so, subject to the following conditions:
dnl
dnl The above copyright notice and this permission notice shall be included in all
dnl copies or substantial portions of the Software.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
dnl IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
dnl FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
dnl AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
dnl LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
dnl OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
dnl SOFTWARE.

AC_INIT([Rxfr], [0.6.4], [andrewds@usc.edu],
        [Rxfr], [https://github.com/andrewdavidsmith/transferase])

## AM_INIT_AUTOMAKE([subdir-objects foreign])

AC_CONFIG_MACRO_DIR([m4])
AC_LANG(C++)
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX([23], [noext], [mandatory])
AC_PROG_RANLIB

# Preserve any user CPPFLAGS and add your own include path
RXFR_CPPFLAGS="${CPPFLAGS} -I."
AC_SUBST([RXFR_CPPFLAGS])

# Initialize RXFR_LIBS variable
RXFR_LIBS=""

# Check for OpenSSL (libssl), fail if missing
AC_CHECK_HEADERS([openssl/ssl.h], [],
  [AC_MSG_ERROR([openssl/ssl.h header not found. Please install OpenSSL development library.])])
AC_CHECK_LIB([ssl], [SSL_new],
  [RXFR_LIBS="-lssl $RXFR_LIBS"],
  [AC_MSG_ERROR([libssl not found; Please install OpenSSL development library.])])
# Check for libdeflate, fail if missing
AC_CHECK_HEADER([libdeflate.h], [],
  [AC_MSG_ERROR([libdeflate.h header not found. Please install libdeflate.])])
AC_SEARCH_LIBS([libdeflate_deflate_compress], [deflate], [RXFR_LIBS="-ldeflate $RXFR_LIBS"],
               [AC_MSG_ERROR([libdeflate library not found. Please install libdeflate.])])
dnl AC_CHECK_HEADER([libdeflate.h], [],
dnl   [AC_MSG_ERROR([libdeflate.h header not found; please install libdeflate])])
dnl AC_CHECK_LIB([deflate], [deflate_compress],
dnl   [LIBS="-ldeflate $RXFR_LIBS"],
dnl   [AC_MSG_ERROR([libdeflate library not found; please install libdeflate])])

AC_SUBST([RXFR_LIBS])

AC_CONFIG_FILES([src/Makevars])
AC_OUTPUT
