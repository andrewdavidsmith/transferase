# Building from source

Currently this process might seem a bit complicated, so the best way for me to
explain is to walk through the build process on different systems. I probably
will expand this with more systems and update it as the transferase source
stabilizes.

## Ubuntu

I'm using the Ubuntu 24.04 docker image. I will list all the steps, even though most of
them won't be needed on your system. First is to build the command line tools,
then the Python package.

### Command line app

We need the following packages. I will some of them along with the package
names.  I will also indicate the versions I got while writing this, and the
minimum versions that are required, if relevant.

- git and ca-certificates (for using git and https)
- g++: g++-14 (14.2.0/14.2.0)
- Boost: libboost-all-dev (1.83/1.78)
- cmake: cmake (3.28.3/3.28)
- gnu make: make
- ncurses: libncurses5-dev (for the 'select' command)

This next command will install all the packages listed above:

```console
apt-get update && \
apt-get install -y --no-install-recommends \
    ca-certificates \
    git \
    g++-14 \
    libboost-all-dev \
    cmake \
    make \
    libncurses5-dev
```

It might take a few minutes and they install quickly, but one of them might
ask for your timezone. Once they have installed, clone the repo:

```console
git clone https://github.com/andrewdavidsmith/transferase
cd transferase
```

The build configuration needs some variables. I will explain the parts below:

```console
cmake -B build -DCMAKE_CXX_COMPILER=g++-14 -DBUILD_CLI=on -DCMAKE_BUILD_TYPE=Release
```

If you are not familiar with cmake, the variables are set with a `-D`. The
`CMAKE_BUILD_TYPE` is a standard variable, and when set to `Release` makes
faster code. The `CMAKE_CXX_COMPILER` is needed because cmake won't use
`g++-14` if it is not specified. Even if you have a different compiler, be
aware that transferase uses the c++23 standard, so you will need either g++
with at least version 14.2.0, or clang++ with at least version 20.0.0, the
latter not yet available in packages as I write this. The `-B build` tells it
to put all the work inside the `build` directory, so you can easily delete it
all if you made a mistake. Finally, one that is specific to transferase:
`BUILD_CLI=on` builds the command line interface, which you absolutely need,
but is off by default for my own convenience.

Now is time for the build. The following basically runs make. Use `-j` to
specify how many cores you want to use when building:

```console
cmake --build build -j32
```

After it builds, you can install as follows:

```console
cmake --install build --prefix=/usr/local
```

As of now (v0.5.0 just released), the `xfr` alias isn't generated unless
`-DPACKAGE=on` is specified on the `cmake -B ...` command line, but that
should be fixed soon. If you add `-DPACKAGE=on` it will also statically link
most libraries.

### Python package

We will work with a virtual environment to be sure things don't get mixed
up. This means we need another package. But we also need a C compiler because
we will be making ZLib from source. So getting out own :

```console
apt install -y --no-install-recommends python3.12-venv
```

Then make the venv and activate it.

```console
python3 -m venv .venv && . .venv/bin/activate
```

Here are the Python packages we need and how to get them:

```console
pip install nanobind pytest wheel auditwheel hatch
```

You likely won't be using the pytest. The critical one is nanobind, which is
the foundation of making transferase available in Python. Currently
transferase builds its own ZLib to make Python bindings. We need to supply a C
compiler for that. Our cmake process isn't setup to easily pass that along, so
we set the CC env variable before the build (gcc-14 likely came along with
g++-14, but if not, just install it):

```console
export CC=gcc-14
cmake -B build -DCMAKE_CXX_COMPILER=g++-14 -DPACKAGE_PYTHON=on -DCMAKE_BUILD_TYPE=Release
```

There is an option to `BUILD_PYTHON` but it's separate from `PACKAGE_PYTHON`,
which builds the wheel file (for my convenience). You should end up with a
file in `build/python/dist` that has the extension `whl` and that can be
installed with pip. Here is what I got and how to install it (we are still in
a venv):

```
pip install build/python/dist/transferase-0.5.0-cp312-none-manylinux_2_38_x86_64.whl
```

In the above naming, the `cp312` is the Python version. The `none` means no
specific ABI. The `manylinux_2_38_x86_64` means the package can work on any
Linux with GLIBC >= 2.38. If you want to check that the above installation
worked, do this:

```console
python3 -c "from transferase import *; help(transferase)"
```
