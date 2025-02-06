# Transferase

The transferase system for retrieving methylome data from methbase.

The MethBase2 database currently has well over 13,000 high-quality
methylomes for vertebrate species, mostly split equally between human
and mouse. For most epigenomics scientists, answering important
questions requires identifying relevant data sets and obtaining
methylation levels summarized at relavant parts of the genome.
Transferase provides fast access to MethBase2 via queries made up of
two parts: a set of genomic intervals and one or more methylomes of
interest. Queries return methylation levels in each interval for each
specified methylome. There is no need to download entire methylomes
because the server can quickly respond with the methylation levels in
query regions -- tens of thousands of intervals in a single query,
with responses as fast as a few seconds over regular home internet.
In some situations the transferse server can respond faster than you
could load the data files if had them locally on your laptop or
workstation.

- The current version (v0.4.0) is still in an early development
  stage. It works, but hasn't seen enough use to know if there are any
  serious bugs lurking.

- As of v0.4.0, the server is not yet open to the public. Hopefully it
  will be available by sometime in the first half of February 2025.

- If you want to help test transferase, also get to use it early,
  contact me (Andrew) and I can will give you early access.

- If you have a lot of methylome data, whether it's from whole-genome
  bisulfite sequencing, or from Nanopore, you can use transferase
  locally to get very fast data analysis.

- Target systems are Mac and Linux. Currently a binary release is only
  available for Linux; Mac should follow soon.

- Target platforms are command line, Python and R. Currently the
  command line tools work, and the Python API also works, but with
  limited testing.

- The current release has binaries that should work on almost any
  Linux machine. The Python package needs Python 3.12, but otherwise
  will run on almost any Linux machine.


The transferase program has an alias `xfr` (installed as a copy of the
program) which is quicker to type and will be used below. There are
several commands within `xfr` and the best way to start is to
understand the `dnmtools roi` command, as the information
functionality provided by `xfr` is the same. If you need to learn
about `dnmtools roi` you can find the docs
[here](https://dnmtools.readthedocs.io/en/latest/roi)

## Installing transferase

### Install the pre-compiled binary

If you are on a reasonably recent Linux (i.e., no older than 10
yeads), then you can install the binary distribution. First
download it like this:
```console
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.3.0/transferase-0.3.0-Linux.sh
```

Then run the downloaded installer (likely you want to first install it
beneath your home dir):
```console
sh transferase-0.3.0-Linux.sh --prefix=${PREFIX}
```

This will prompt you to accept the license, and then it will install
the transferase binaries in `${PREFIX}/bin`, along with some config files
in `${PREFIX}/share`. If you want to install it system-wide, and have
the admin privs, you can do:
```console
sh transferase-0.3.0-Linux.sh --prefix=/usr/local
```

If you are on Debian or Ubuntu, and have admin privileges, you can use
the Debian package and then transferase will be tracked in the package
management system.  Get the file like this:
```console
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.3.0/transferase-0.3.0-Linux.deb
```

And then install it like this:
```console
sudo dpkg -i ./transferase-0.3.0-Linux.deb
```

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
