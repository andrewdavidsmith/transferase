# Building transferase from source

This document is for anyone who wants to build transferase from source
or who is participating in writing or maintaining the code. Unlike
previous Open Source software I've written, transferase was intended
from the start to be distributed as binaries. Building from source is
not easy (early 2025), and making binaries as intended for
distribution is even harder.

## Dependencies

* CMake
* NCurses
* nanobind
* Boost
* ZLib (for Python)

The transferase program has an alias `xfr` (installed as a copy of the
program) which is quicker to type and will be used below. There are
several commands within `xfr` and the best way to start is to
understand the `dnmtools roi` command, as the information
functionality provided by `xfr` is the same. If you need to learn
about `dnmtools roi` you can find the docs
[here](https://dnmtools.readthedocs.io/en/latest/roi)


### Building the source

Not recommended unless you know what you are doing. You will need the
following:

* A compiler that can handle most of C++23, one of
  - GCC >= [14.2.0](https://gcc.gnu.org/pub/gcc/releases/gcc-14.2.0/gcc-14.2.0.tar.gz)
  - Clang >= [20.0.0](https://github.com/llvm/llvm-project.git) (no release as of 12/2024)
* Boost >= [1.85](https://archives.boost.io/release/${BOOST_VERSION}/source/boost_1_85.tar.bz2)
* CMake >= [3.30](https://github.com/Kitware/CMake/releases/download/v3.30.2/cmake-3.30.2.tar.gz)
* ZLib, any version, just install it with `apt install zlib1g-dev`,
  `mamba install -c conda-forge zlib`, etc. From source is fast and
  easy.

However you install these, remember where you put them and update your
paths accordingly.

Since transferase uses CMake to generate the build system, there are
multiple ways to do it, but I like this:
```shell
tar -xf transferase-0.3.0-Source.tar.gz
cd transferase-0.3.0-Source
cmake -B build -DCMAKE_BUILD_TYPE=Release   # for a faster xfr
cmake --build build -j64      # i.e., if you have 64 cores
cmake --install build --prefix=${HOME}  # or wherever
```
