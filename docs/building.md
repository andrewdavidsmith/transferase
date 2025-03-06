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

#### Dependencies

We need the following packages. I will some of them along with the package
names.  I will also indicate the versions I got while writing this, and the
minimum versions that are required, if relevant.

- git and ca-certificates (for using git and https)
- g++: g++-14 (14.2.0/14.2.0)
- Boost: libboost-all-dev (1.83/1.78)
- cmake: cmake (3.28.3/3.28)
- gnu make: make
- ncurses: libncurses5-dev (for the 'select' command)

This next command will install all the packages listed above, and the
`DEBIAN_FRONTEND=noninteractive` is to prevent from being asked your timezone
in the process:

```console
apt-get update && \
DEBIAN_FRONTEND=noninteractive \
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
ask for your timezone.

#### Building

Once they have installed, clone the repo:

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
dpkg --install build/transferase-0.5.0-Linux.deb
dpkg --status transferase
dpkg --remove transferase
```

Or `apt` (note how the path is formed):

```console
apt install ./build/transferase-0.5.0-Linux.deb
apt info transferase
apt remove transferase
```

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

```console
pip install build/python/dist/transferase-0.5.0-cp312-none-manylinux_2_38_x86_64.whl
```

In the above naming, the `cp312` is the Python version. The `none` means no
specific ABI. The `manylinux_2_38_x86_64` means the package can work on any
Linux with GLIBC >= 2.38. If you want to check that the above installation
worked, do this:

```console
python3 -c "from transferase import *; help(transferase)"
```

## Fedora

I'm using the Fedora version 41 docker image. I will do this slightly
differently than the Ubuntu instructions above, and focus on building using
mostly static libraries (except the GLIBC), and build the "package" for
transferase.

### Command line app

We need mostly the same packages as for Ubuntu. I will list them again here,
along with some version information for the dnf packages I found.

- git: git
- g++: g++ (14.2.1/14.2.0)
- Boost: boost-devel (1.83/1.78)
- cmake: cmake (3.30.8/3.28)
- ncurses: ncurses-devel (for the 'select' command)
- ZLib: zlib-devel

The above can be installed as follows:

```console
dnf update -y && \
dnf install -y \
    git \
    g++ \
    cmake \
    zlib-devel \
    boost-devel \
    ncurses-devel
```

Similar to case for Ubuntu, we clone transferase, configure the build and then
run it. Notice that on Fedora we use `g++` and not `g++-14`:

```console
git clone https://github.com/andrewdavidsmith/transferase
cd transferase
cmake -B build -DCMAKE_CXX_COMPILER=g++ -DBUILD_CLI=on -DCMAKE_BUILD_TYPE=Release
cmake --build build -j32
```

#### Installing and packaging

The transferase packages could be built on Ubuntu using the same dependencies
as for a regular build. Fedora is different. In order to build with
`-DPACKAGE=on` we need to get static libraries for most dependencies:

- zlib-static: without this, the build configuration fails
- boost-static: without this, the failure is at the final link step
- libstdc++-static: similarly, without this the build fails at link time
- ncurses-static: needed for the 'select' command to build

We also need the `rpm-build` package. Here is the command:

```console
dnf install -y \
    zlib-static \
    boost-static \
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
dnf install build/transferase-0.5.0-Linux.rpm
dnf info transferase
dnf remove transferase
```

Or with `rpm`:

```console
rpm -i build/transferase-0.5.0-Linux.rpm
rpm -qi transferase  # Info
rpm -e transferase  # Remove
```
