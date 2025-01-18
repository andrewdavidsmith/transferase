# Transferase

The transferase system for retrieving methylomes from methbase.

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

## Commands

### Make an index file

Before starting, an index file is required. To make the index, you
need the reference genome in a single fasta format file. If your
reference file name is `hg38.fa`, then do this:
```console
xfr index -v -g hg38.fa -x hg38.cpg_idx
```

If `hg38.fa` is roughly 3.0G in size, then you should expect the index
file `hg38.cpg_idx` to be about 113M in size. This command will also
create an "index metadata" file `hg38.cpg_idx.json`, which is named by
just adding the `.json` extension to the provided output file. You can
also start with a gzip format file like `hg38.fa.gz`.

### Make a methylome file

The starting point within `xfrase` is a file in the `xcounts` format that
involves the symmetric CpG sites. This is called a symmetric xcounts
format file, and may be gzip compressed, in which case the extension
should be `.xsym.gz`. First I will explain how to start with this
format, and below I will explain how to start with the other format
used by `dnmtools`. We again assume that the reference genome is
`hg38.fa` and that this file was used to create the
`SRX012345.xsym.gz` file. This ensures a correspondence between the
`SRX012345.xsym.gz` file and the `hg38.cpg_idx` file we will need.
Here is how to convert such a file into the xfrase format:
```console
xfrase compress -v -x hg38.cpg_idx -m SRX012345.xsym.gz -o SRX012345.m16
```

As with the `hg38.cpg_idx` index file, the methylome file
`SRX012345.m16` will be accompanied by a metadata file with an
additional json extension: `SRX012345.m16.json`.

If you begin with a
[counts](https://dnmtools.readthedocs.io/en/latest/counts) format
file, for example `SRX012345.sym`, created using the `dnmtools counts`
and then `dnmtools sym` commands, then you will need to first convert
it into `.xsym` format (whether gzipped or not). You can do this as
follows:
```console
dnmtools xcounts -z -o SRX012345.xsym.gz -c hg38.fa -v SRX012345.sym
```

Once again, be sure to always use the same `hg38.fa` file.  A hash is
generated and used internally to `xfrase` to ensure that the index and
methylome files correspond to the same reference genome file.

### Run the `intervals` command locally

This step is to make sure everything is sensible. Or you might just
want to keep using this tool for your own analysis (it's fast). You
will need a set of genomic intervals of interest. In this example
these will be named `intervals.bed`. You also need the index file and
the methylome file explained in the above steps.
```console
xfrase intervals local -v debug -x hg38.cpg_idx -m SRX012345.m16 -o local_output.bed -i intervals.bed
```

The index file `hg38.cpg_idx` and the methylome file `SRX012345.m16`
are the same as explained above. The `intervals.bed` file may contain
any number of columns, but the first 3 columns must be in 3-column BED
format: chrom, start, stop for each interval.  The output in the
`local_output.bed` file should be consistent with the information in
the command:
```console
dnmtools roi -o intervals.roi intervals.bed SRX012345.xsym.gz
```

The format of these output files are different, but the methylation
levels on each line (i.e., for each interval) should be identical.
Note that `dnmtools roi` can fail if intervals are nested, while
`xfrase intervals` command will still work.

### Run the `server` command

The `server` command can be tested first locally by using two terminal
windows. We require the index file `hg38.cpg_index` and the methylome
file `SRX012345.m16` along with the corresponding `.json` metadata
files. These should be in directories named for methylomes and
indexes. The methylomes directory will contain all files with `.m16`
and `.m16.json` extensions; these are the methylomes that can be
served. The indexes directory will contain the files ending with
`.cpg_idx` and `.cpg_idx.json`; these are index files for each
relevant genome assembly. For now, using the above examples, we
would have a single index and a single methylome. I will assume these
are in subdirectories, named `indexes` and `methylomes` respectively,
of the current directory. Here is a command that will start the server:
```console
xfrase server -v debug -s localhost -p 5000 -m methylomes -x indexes
```

Not that this will fail with an appropriate error message if port 5000
is already be in use, and you can just try 5001, etc., until one is
free. The `-v debug` will ensure you see info beyond just the
errors. This informtion is logged to the terminal by default.

### Run the `intervals` command remotely

We will assume for now that "remote" server is running on the local
machine (localhost) and using port is 5000 (the default). The
following command should give identical earlier `intervals` command:
```console
xfrase intervals remote -v debug -s localhost -x indexes/hg38.cpg_idx \
    -o remote_output.bed -a SRX012345 -i intervals.bed
```

Note that now `SRX012345` is not a file this time. Rather, it is a
methylome name or accession, and should be available on the server. If
the server can't find the named methylome, it will respond indicating
that methylome is not available.
