# Docker containers for mxe

On Ubuntu base systems, and probably most others, zlib is not
available by default so it can be installed using:
```console
apt-get install -y zlib1g-dev
```

Mxe is written using the C++23 standard as implemented in GCC 14.2 and
Clang 20. As of late November 2024 (time of writing), these are not
available except by building from source. The minimum version of Boost
that is required is currently 1.85.0. As of late November 2024, the
version available by default with `apt` is 1.74.0. Similarly, CMake
3.30 is needed for building mxe, and it is not easily available. The
current recommendation is to use the following docker container:
```console
andrewdavidsmith/gcc-cmake-boost
```
as it includes suitable versions of GCC, CMake and Boost. You will
need to use `apt` to get missing dependencies, including `zlib1g-dev`
for `zlib` and `ca-certificates` for `git`.
