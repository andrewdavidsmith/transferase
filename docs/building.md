# Building from source

Currently this process might seem a bit complicated, so the best way for me to
explain is to walk through the build process on different systems. I probably
will expand this with more systems and update it as the transferase source
stabilizes. You might find what seems like unnecessary redundancy in the steps
below, but that's mostly because I copy-paste from this document, and want to
keep things together.

## Ubuntu

These instructions assume Ubuntu 24.04 and I'm using the Ubuntu 24.04 docker
image. I will list all the steps, even though most of them won't be needed on
your system. First is to build the command line tools, then the Python
package.

### Command line app

#### Dependencies

This next command will install all the required packages, most of which you
probably already have. The `DEBIAN_FRONTEND=noninteractive` is to prevent from
being asked your timezone in the process:

```console
apt-get update && \
DEBIAN_FRONTEND=noninteractive \
apt-get install -y --no-install-recommends \
    ca-certificates \
    git \
    g++-14 \
    cmake \
    make \
    libncurses5-dev \
    libssl-dev \
    zlib1g-dev \
    libdeflate-dev
```

#### Building

Once they have installed, clone the repo:

```console
git clone https://github.com/andrewdavidsmith/transferase && \
cd transferase
```

The build configuration needs some variables. I will explain the parts below:

```console
cmake -B build -DCMAKE_CXX_COMPILER=g++-14 -DCMAKE_BUILD_TYPE=Release
```

If you are not familiar with cmake, the variables are set with a `-D`. The
`CMAKE_BUILD_TYPE` is a standard variable, and when set to `Release` makes
faster code. The `CMAKE_CXX_COMPILER` is needed because cmake won't use
`g++-14` if it is not specified. Even if you have a different compiler, be
aware that transferase uses the c++23 standard, so you will need either g++
with at least version 14.2.0, or clang++ with at least version 20.0.0, the
latter not yet available in packages as I write this (Note: LLVM Clang 20 is
available in packages as of 04/2025). The `-B build` tells it to put all the
work inside the `build` directory, so you can easily delete it all if you made
a mistake.

Now is time for the build. The following basically runs make. Use `-j` to
specify how many cores you want to use when building:

```console
cmake --build build -j32
```

You can probably benefit from this many cores, but more likely won't help.

#### Installing and packaging

After it builds, you can install beneath your home directory as follows:

```console
cmake --install build --prefix=$HOME
```

This will put files in `bin`, `share` and `lib` directories, creating them if
they aren't already there. Note: You probably don't want to do this if you are
installing system-wide. There will be a file name `install_manifest.txt` in
the `build` directory indicating all files that were installed, but you
probably want a package manager to deal with these things for you.

You can build "packages" for transferase, but the build configuration will
need to be slightly different. Configure as follows:

```console
cmake -B build -DCMAKE_CXX_COMPILER=g++-14 -DPACKAGE=on -DCMAKE_BUILD_TYPE=Release
```

And build as previously:

```console
cmake --build build -j32
```

But after this step, the following will create several kinds of packages
in the `build` directory:

```console
cpack -B build --config build/CPackConfig.cmake
```

On Ubuntu, the package you want is the DEB package (packages for other systems
might be generated at the same time). You can copy it out of the build tree,
save it somewhere, etc. This transferase DEB currently doesn't conform to all
the guidelines, but it's enough to allow you to or anyone else with privileges
on your system to cleanly uninstall it. You can use `dpkg`:

```console
# Examine what will be installed
dpkg --info build/transferase-0.6.3-Linux.deb
# Do the install
dpkg --install build/transferase-0.6.3-Linux.deb
# See what has been installed
dpkg --status transferase
# Remove it
dpkg --remove transferase
```

Or `apt` (note how the path is formed):

```console
apt install ./build/transferase-0.6.3-Linux.deb
apt info transferase
apt remove transferase
```

### Python package

We will work with a virtual environment to be sure things don't get mixed
up. This means we need two more packages, one for building Python things (dev)
and one for thee virtual env (venv):

```console
DEBIAN_FRONTEND=noninteractive \
apt-get install -y --no-install-recommends \
    python3.12-dev \
    python3.12-venv
```

Then make the venv and activate it.

```console
python3.12 -m venv .venv && . .venv/bin/activate
```

Here are the Python packages we need and how to get them:

```console
pip install nanobind pytest wheel auditwheel hatch numpy
```

You likely won't be using the pytest. The critical one is nanobind, which is
the foundation of making transferase available in Python. Currently
transferase builds its own ZLib to make Python bindings. We need to supply a C
compiler for that. Our cmake process isn't setup to easily pass that along, so
we set the CC env variable before the build (gcc-14 likely came along with
g++-14, but if not, just install it):

```console
export CC=gcc-14 && \
cmake -B build -DCMAKE_CXX_COMPILER=g++-14 -DPACKAGE_PYTHON=on -DCMAKE_BUILD_TYPE=Release && \
cmake --build build -j32
```

There is an option to `BUILD_PYTHON` but it's separate from `PACKAGE_PYTHON`,
which builds the wheel file (for my convenience). You should end up with a
file in `build/python/dist` that has the extension `whl` and that can be
installed with pip. Here is what I got and how to install it (we are still in
a venv):

```console
pip install build/python/dist/pyxfr-0.6.3-cp312-none-manylinux_2_38_x86_64.whl
```

In the above naming, the `cp312` is the Python version. The `none` means no
specific ABI. The `manylinux_2_38_x86_64` means the package can work on any
Linux with GLIBC >= 2.38. If you want to check that the above installation
worked, do this:

```console
python3 -c "from pyxfr import *; help(pyxfr)"
```

Full Python build, from dependencies to test, in Ubuntu 24.04 is the
following. Note that the final built filename
(`pyxfr-0.6.3-cp312-none-manylinux_2_38_x86_64.whl`) depends on using Ubuntu
24.04 and all the earlier steps:

```console
apt-get update && \
DEBIAN_FRONTEND=noninteractive \
apt-get install -y --no-install-recommends \
    ca-certificates \
    git \
    g++-14 \
    gcc-14 \
    cmake \
    make \
    libssl-dev \
    python3.12-dev \
    python3.12-venv && \
python3.12 -m venv .venv && . .venv/bin/activate && \
pip install nanobind pytest wheel auditwheel hatch numpy && \
git clone https://github.com/andrewdavidsmith/transferase && \
cd transferase && \
export CC=gcc-14 && \
cmake -B build -DCMAKE_CXX_COMPILER=g++-14 \
    -DPACKAGE_PYTHON=on -DCMAKE_BUILD_TYPE=Release && \
cmake --build build -j32 && \
pip install build/python/dist/pyxfr-0.6.3-cp312-none-manylinux_2_38_x86_64.whl && \
python3 -c "from pyxfr import *; help(pyxfr)"
```

## Fedora

I'm using the Fedora version 41 docker image. I will do this slightly
differently than the Ubuntu instructions above, and focus on building using
mostly static libraries (except the GLIBC), and build the "package" for
transferase.

### Command line app

The dependencies can be installed through the dnf packages as follows:

```console
dnf update -y && \
dnf install -y \
    git \
    g++ \
    cmake \
    zlib-devel \
    libdeflate-devel \
    ncurses-devel \
    openssl-devel
```

Similar to case for Ubuntu, we clone transferase, configure the build and then
run it. Notice that on Fedora we use `g++` and not `g++-14`:

```console
git clone https://github.com/andrewdavidsmith/transferase
cd transferase
cmake -B build -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release
cmake --build build -j32
```

#### Installing and packaging

The transferase packages could be built on Ubuntu using the same dependencies
as for a regular build. Fedora is different. In order to build with
`-DPACKAGE=on` we need to get static libraries for most dependencies:

- zlib-static: without this, the build configuration fails
- libstdc++-static: similarly, without this the build fails at link time
- ncurses-static: needed for the 'select' command to build

We also need the `rpm-build` package. Here is the command:

```console
dnf install -y \
    zlib-static \
    ncurses-static \
    libstdc++-static \
    rpm-build
```

And we build the RPM as follows:

```console
cmake -B build -DCMAKE_CXX_COMPILER=g++ -DPACKAGE=on -DCMAKE_BUILD_TYPE=Release
cmake --build build -j32
cpack -B build --config build/CPackConfig.cmake
```

You can install/remove the RPM with `dnf`:

```console
dnf install build/transferase-0.6.3-Linux.rpm
dnf info transferase
dnf remove transferase
```

Or with `rpm`:

```console
rpm -i build/transferase-0.6.3-Linux.rpm
rpm -qi transferase  # Info
rpm -e transferase  # Remove
```

## The R API

**(Note: these instructions work on Ubuntu 25.04, released on 2025-05-17)**

The transferase API for R is called Rxfr.  In mid-March 2025, I'm using the
unstable ("sid") Debian docker image to build the R API for transferase
because it has R-4.4 and GCC-14.2 by default. By the end of April, 2025,
Ubuntu should have these in a main release. Start the container like this:

```console
docker run --rm -it debian:sid bash
```

Then get the required packages like this:

```console
apt-get update && \
DEBIAN_FRONTEND=noninteractive \
apt-get install -y --no-install-recommends \
    ca-certificates \
    git \
    gcc \
    g++ \
    cmake \
    make \
    libssl-dev \
    libxml2-dev \
    r-base-dev
```

The `libxml2-dev` package installed through `apt-get` is needed by `roxygen2`,
which in a strict sense is optional.

The transferase R API needs the Rcpp package for the build step, and the R6
package to run. It also needs to be able to link to OpenSSL, which is among
the dependencies in the list above. Here's how we get the R packages:

```console
R -e "install.packages(c('Rcpp', 'R6', 'roxygen2'), repos = 'https://cloud.r-project.org')"
```

`roxygen2` is needed to generate the documentation if you building the
transferase R API from source.

As usual, we need to clone the repo:

```console
git clone https://github.com/andrewdavidsmith/transferase && \
cd transferase
```

The build configuration needs an install prefix because we will be skipping
the usual `--build` step in cmake. Our goal is to make a source tree that
conforms to what R wants, so R can build it in the best way to work with R.

```console
cmake -B build -DBUILD_R=on -DCMAKE_INSTALL_PREFIX=Rxfr -Wno-dev
```

The `-Wno-dev` is to avoid warnings, and might not be needed in a few months.

The `build` and `Rxfr` directories are arbitrary, but make sense. Depending on
what else I'm doing at the same time, I might use `build_r` and `src`,
respectively, for those names. Just don't get these two confused. However, you
name them, neither should exist before you run the above command. After
running the above command, the `build` directory should exist.

The next step is to "install" into a modified source tree for R:

```console
cmake --install build
```

This will create a directory named `Rxfr` if you followed my commands
exactly. This step uses `Rcpp` to generate boilerplate code that exposes
transferase functions in R. To satisfy the requirements on R code, the
transferase source files are also modified in several different ways.

Once this new source tree has been created, you can directly install it.  This
means no documentation, though. If you want to do that, this is the command:

```console
MAKEFLAGS="-j32" R CMD INSTALL Rxfr
```

Remember, the "Rxfr" was a name we chose, not any kind of keyword or
convention. The name of the package is not determined by our use of "Rxfr"
here, but instead by a field within the `DESCRIPTION` file inside that
directory. Replace the 32 above with whatever number of cores you want to
use. In this case, if you have 32 cores, the install will be faster. The above
step will likely install transferase in your local packages directory, which
for me is `${HOME}/R/x86_64-pc-linux-gnu-library/4.4`. If you are in a
container, or working in admin mode, it might put them in a system location
like `/usr/local/lib/R/site-library`.

Building the documentation, along with an archive that others can install, is
more involved. I'm trying to follow guidelines that will eventually make
transferase fully conformant with CRAN rules. The documentation for
transferase in R needs to be build with roxygen2. Unfortunately, due to how
Rcpp and roxygen2 interact, the least unpleasant way to generate the
documentation involves building everything twice.

We start by building the shared library so that roxygen2 won't attept to do it
(it would fail). The following command will put object files (`.o`) and a
shared library file (`.so`) in the `Rxfr/src` directory, but will not actually
install them anywhere:

```console
MAKEFLAGS="-j32" R CMD INSTALL --no-inst Rxfr
```

Now we can generate the documentation files for the individual functions and
classes in the package:

```console
R -e "library(roxygen2, R6); roxygen2::roxygenize('Rxfr')"
```

This will generate multiple files named like `*.Rd` in the `Rxfr/man`
directory. These are used when you do `? function_name` in your R
interpreter. Next we need the "manual" which is a pdf document required by
CRAN. To get this we need to latex, along with a specific font used by R:

```console
DEBIAN_FRONTEND=noninteractive \
apt-get install -y --no-install-recommends \
    texlive \
    texlive-fonts-extra
```

Now we generate the pdf manual from the individual Rd help files:

```console
mkdir Rxfr/doc && \
R CMD Rd2pdf -o Rxfr/doc/Rxfr.pdf --no-preview Rxfr
```

I added the `--no-preview` because working inside a docker container, I have
no way to view the pdf. R expects the manual to be in the `Rxfr/doc` if the
package is in `Rxfr`.

Note: we created `.o` and `.so` files previously, but R will ignore these in
subsequent steps, so there is no need to delete them.

Now we make the package archive:

```console
R CMD build Rxfr
```

The `build` above is a sub-command to `R CMD` and not the name of a CMake
build directory. This command generates a file named `Rxfr_0.6.3.tar.gz`
(unless I forgot to update these docs with a new version number...).

The final step tells us how well we did by running the "check" command:

```console
MAKEFLAGS="-j32" R CMD check Rxfr_0.6.3.tar.gz
```

Since the above command will build all the code, using the `-j32` helps with
speed.

If the above command works without "warnings", congratulate both yourself and
me. Now you can use the `Rxfr_0.6.3.tar.gz` as follows to install:

```R
install.packages("Rxfr_0.6.3.tar.gz")
library(Rxfr)
library(help=Rxfr)
```

Notes:

- You can export `DEBIAN_FRONTEND` to avoid specifying it every time. It can
  be a big deal because you might start an install and come back 15 min later
  only to find it didn't even begin due to trying to ask you about timezones.

- Similarly, if you export `MAKEFLAGS` to set multiple cores, it help wherever
  it can. In `R CMD`, inside `R` and even building the docs in latex. Just
  unset it if you ever get complaints about regular make being confused. If
  confused it never hurts, but it can dump more text on your screen.

- In `debian:sid` (I haven't tried stable Debian) I need to set the locale to
  avoid a "warning" from `R CMD check`: `export LANG=C.UTF-8
  LC_ALL=C.UTF-8`. I didn't notice the same issue on my own Ubuntu machines,
  but each of those is a mess.

- I get a "Note" from `R CMD check` because the `Rxfr.so` shared library is
  over 50MB in size (!). Interestingly, if I build without `-g` the size
  decreases down to roughly 2MB. There are many ways to make the shared
  library smaller, and I expect to ensure users get small binaries eventually.
  But in a process that ends with `R CMD check` I want to modify as few build
  settings as possible. I don't see the same issue with default settings
  building with R as installed through Homebrew on macOS.

All together as one copy-paste:

```console
export DEBIAN_FRONTEND=noninteractive MAKEFLAGS="-j32" LANG=C.UTF-8 LC_ALL=C.UTF-8 && \
apt-get update && \
apt-get install -y --no-install-recommends \
    ca-certificates \
    git \
    gcc \
    g++ \
    cmake \
    make \
    libdeflate-dev \
    libssl-dev \
    libxml2-dev \
    r-base-dev \
    texlive \
    texlive-fonts-extra && \
R -e "install.packages(c('Rcpp', 'R6', 'roxygen2'), repos = 'https://cloud.r-project.org')"
git clone https://github.com/andrewdavidsmith/transferase && \
cd transferase && \
cmake -B build -DBUILD_R=on -DCMAKE_INSTALL_PREFIX=Rxfr -Wno-dev && \
cmake --install build && \
R CMD INSTALL --no-inst Rxfr && \
R -e "library(roxygen2, R6); roxygen2::roxygenize('Rxfr')" && \
mkdir Rxfr/doc && \
R CMD Rd2pdf -o Rxfr/doc/Rxfr.pdf --no-preview Rxfr && \
R CMD build Rxfr && \
R CMD check Rxfr_0.6.3.tar.gz
```

## Building for tests

Unit tests are available for the back-end library, the command line tools, and
the Python bindings. Integration tests are available but kept in a separate
GitHub repo because of the data size requirements.

### Unit tests

To build for unit tests in Ubuntu, we additionally need GoogleTest, which we
can get through apt:

```console
apt-get update && \
DEBIAN_FRONTEND=noninteractive \
apt-get install -y --no-install-recommends \
    ca-certificates \
    git \
    g++-14 \
    cmake \
    make \
    libncurses5-dev \
    libssl-dev \
    zlib1g-dev \
    libdeflate-dev \
    libgtest-dev
```

The build configuration needs to turn on `UNIT_TESTS`. Important: the tests
will not work if `CMAKE_BUILD_TYPE` is set to `Release` because that sets
`NDEBUG` (this is a general thing, not specific to transferase). This turns
off code that exposes internal functions to the test stubs, and you would get
a compile error. You do not need to set `CMAKE_BUILD_TYPE` to `Debug`,
however.  So here is the build config command and the build command itself:

```console
cmake -B build -DCMAKE_CXX_COMPILER=g++-14 -DUNIT_TESTS=on -DCMAKE_BUILD_TYPE=Build &&
cmake --build build
```

This will generate tests for both the library and the command line tools. To
run unit tests for the library, do this:

```console
ctest --test-dir build/lib
```

Hopefully you will see >150 tests passing. The tests for the cli tools give
arguments directly to each command, and do not run through the
`transferase`/`xfr` binaries:

```console
ctest --test-dir build/cli
```

There are tests that need the network, so if you have an environment that
can't reach external servers, like `example.com`, then at least some will
fail. These do not currently check for internet availability and will fail if
they can't reach arbitrary hosts.

### Python unit tests

Python unit tests are run through `pytest` and they are unfortunately few at
this time. The most reliable way to run the tests is to build for installing
the Python API and for running the Python unit tests. That means setting both
`-DPACKAGE_PYTHON=on` and `-DPYTHON_TESTS=on`. Then once the build is
finished, installing transferase with pip before running the tests.

```console
DEBIAN_FRONTEND=noninteractive \
apt-get install -y --no-install-recommends \
    python3.12-dev \
    python3.12-venv \
    libgtest-dev
```

With the above dependencies installed, these commands will do the build
config, the build, the installation with pip and the pytest.

```console
# Create and activate the venv (need Python >= 3.12)
python3 -m venv .venv && . .venv/bin/activate

# Install the Python dependencies
pip install nanobind pytest wheel auditwheel hatch numpy

# Export 'CC' as cmake builds zlib from source, but won't try to use gcc-14 if
# we specify it as CMAKE_C_COMPILER
export CC=gcc-14

# Configure the transferase build for Python install and test
cmake -B build -DCMAKE_CXX_COMPILER=g++-14 \
    -DPACKAGE_PYTHON=on -DLIB_ONLY=on -DPYTHON_TESTS=on -DCMAKE_BUILD_TYPE=Build

# Do the build
cmake --build build -j32

# Install the Python package
pip install build/python/dist/pyxfr-*.whl

# Run the pytest unit tests
pytest --rootdir=build/python/test -v -x build/python
```

And starting from a fresh image:

```console
# Get system dependencies
apt-get update && \
DEBIAN_FRONTEND=noninteractive \
apt-get install -y --no-install-recommends \
    ca-certificates \
    git \
    g++-14 \
    gcc-14 \
    cmake \
    make \
    libssl-dev \
    python3.12-dev \
    python3.12-venv \
    libgtest-dev
# Get Python dependencies
python3.12 -m venv .venv && . .venv/bin/activate && \
pip install nanobind pytest wheel auditwheel hatch numpy
# Download transferase
git clone https://github.com/andrewdavidsmith/transferase && \
cd transferase
# Set the C compiler for building zlib with fpic
export CC=gcc-14
# Configure the build
cmake -B build -DCMAKE_CXX_COMPILER=g++-14 \
    -DPACKAGE_PYTHON=on -DLIB_ONLY=on -DPYTHON_TESTS=on -DCMAKE_BUILD_TYPE=Build
# Do the build
cmake --build build -j32
# Install the Python package (this catches whatever tags the whl file is given)
pip install build/python/dist/pyxfr-*.whl
# Run the pytest unit tests
pytest --rootdir=build/python/test -v -x build/python
```

## Ubuntu 25.04 (2025-04-17)

The instructions here are updated for the
[Ubuntu 25.04](https://hub.docker.com/_/ubuntu) docker image, which is current
the one labeled "rolling" as of 2025-04-17. If you are in a container, and
don't worry about messing up your environment, just copy-paste all these at
once. Otherwise please go line-by-line.

* Command line app: Full instructions. Notable difference wrt 24.04 is that
  we use default g++ on Ubuntu 25.04:

  ```console
  # Get dependencies
  apt-get update && \
  DEBIAN_FRONTEND=noninteractive \
  apt-get install -y --no-install-recommends \
      ca-certificates \
      git \
      g++ \
      cmake \
      make \
      libncurses5-dev \
      libssl-dev \
      zlib1g-dev \
      libdeflate-dev
  # Download transferase
  git clone https://github.com/andrewdavidsmith/transferase && \
  cd transferase
  # Configure the build
  cmake -B build -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release
  # Build the software (-j indicates the number of cores to use)
  cmake --build build -j32
  # Install to $HOME; change to wherever you want to install
  cmake --install build --prefix=$HOME
  ```

* Python package: Notable difference wrt 24.04 is that we use default gcc/g++
  on Ubuntu 25.04, and python3.13.

  ```console
  # Get dependencies (need Python, don't need zlib or ncurses)
  apt-get update && \
  DEBIAN_FRONTEND=noninteractive \
  apt-get install -y --no-install-recommends \
      ca-certificates \
      git \
      gcc \
      g++ \
      cmake \
      make \
      libssl-dev \
      libdeflate-dev \
      python3.13-dev \
      python3.13-venv
  # Install Python dependencies
  python3.13 -m venv .venv && . .venv/bin/activate
  pip install nanobind pytest wheel auditwheel hatch numpy
  # Download transferase
  git clone https://github.com/andrewdavidsmith/transferase && \
  cd transferase
  # Building zlib with fPIC needs CC, but our CMake code doesn't handle it yet
  export CC=gcc && \
  cmake -B build -DCMAKE_CXX_COMPILER=g++ -DPACKAGE_PYTHON=on \
      -DCMAKE_BUILD_TYPE=Release && \
  cmake --build build -j32
  pip install build/python/dist/pyxfr-0.6.3-cp313-none-manylinux_2_38_x86_64.whl
  # Test that it worked
  python3 -c "from pyxfr import *; help(pyxfr)"
  ```
