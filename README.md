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
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.4.0/transferase-0.4.0-Linux.sh
```

Then run the downloaded installer (likely you want to first install it
beneath your home dir):
```console
sh transferase-0.4.0-Linux.sh --prefix=${PREFIX}
```

This will prompt you to accept the license, and then it will install
the transferase binaries in `${PREFIX}/bin`, along with some config files
in `${PREFIX}/share`. If you want to install it system-wide, and have
the admin privs, you can do:
```console
sh transferase-0.4.0-Linux.sh --prefix=/usr/local
```

If you are on Debian or Ubuntu, and have admin privileges, you can use
the Debian package and then transferase will be tracked in the package
management system.  Get the file like this:
```console
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.4.0/transferase-0.4.0-Linux.deb
```

And then install it like this:
```console
sudo dpkg -i ./transferase-0.4.0-Linux.deb
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
tar -xf transferase-0.4.0-Source.tar.gz
cd transferase-0.4.0-Source
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

The starting point is a file in the `xcounts` format that involves the
symmetric CpG sites. This is called a symmetric xcounts format file,
and may be gzip compressed, in which case the extension should be
`.xsym.gz`. First I will explain how to start with this format, and
below I will explain how to start with the other format used by
`dnmtools`. We again assume that the reference genome is `hg38.fa` and
that this file was used to create the `SRX012345.xsym.gz` file. This
ensures a correspondence between the `SRX012345.xsym.gz` file and the
genome index (two files named `hg38.cpg_idx` and `hg38.cpg_idx.json`).
Here is how to convert such a file into the transferase format:
```console
xfr format -x index_dir -g hg38 -o methylome_dir -m SRX012345.xsym.gz
```
As with the genome index, the methylome in transferase format will
involve two files: `SRX012345.m16` and `SRX012345.m16.json`, the
latter containing metadata.

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
generated and used internally to transferase to ensure that the index
and methylome files correspond to the same reference genome file.

### Run the query command locally

This step is to make sure everything is sensible. Or you might just
want to keep using this tool for your own analysis (it can be
extremely fast). We will assume first that you have a set of genomic
intervals of interest. In this example these will be named
`intervals.bed`. You also need the genome index and the methylome
generated in the above steps.
```console
xfr query --local \
    -x index_dir -g hg38 -d methylome_dir -m SRX012345 \
    -o output.bed -i intervals.bed
```

The genome index for hg38 and the methylome for SRX012345 are the same
as explained above (each is two files). The `intervals.bed` file may
contain any number of columns, but the first 3 columns must be in
3-column BED format: chrom, start, stop for each interval.  The output
in the `local_output.bed` file should be consistent with the
information in the command:
```console
dnmtools roi -o intervals.roi intervals.bed SRX012345.xsym.gz
```

The format of these output files are different, but the methylation
levels on each line (i.e., for each interval) should be identical.
Note that `dnmtools roi` can fail if intervals are nested, while
`xfr query` command will still work.

### Running a transferase server

The `server` command can be tested first locally by using two terminal
windows. The steps here assume you have the genome index for hg38 and
a methylome named SRX012345. If you have some other methylome it's
fine, just substitute the name. These files should be in directories
named `methylomes` and `indexes` for this example. The methylome
directory will contain files with `.m16` and `.m16.json` extensions;
these correspond to the methylomes that can be served and must be in
pairs. For every `.m16` file there must be a `.m16.json` file, and
vice-versa. The indexes directory will contain the files ending with
`.cpg_idx` and `.cpg_idx.json`; these are index files for each
relevant genome assembly. For now, using the above examples, we would
have a single index and a single methylome. Here is a command that
will start the server:

```console
xfr server -v debug -s localhost -p 5000 -m methylomes -x indexes
```

Above, the arguments indicate to use the host localhost with
port 5000. Note that this will fail with an appropriate error message
if port 5000 is already be in use, and you can just try 5001, etc.,
until one is free. The hostname can also be specified numerically (in
this example, 127.0.0.1). The `-v debug` will ensure you see info
beyond just the errors. This informtion is logged to the terminal by
default. The server can also be run in detached mode, but if you don't
know what that means, you don't likely have any reason to do it.

### Run the query command remotely

We will assume for now that "remote" server is running on the local
machine (localhost) and using port is 5000 (the default). You would
have started this in the previous step using a different terminal
window. The following command should give identical results to the
earlier `xfr query` command:

```console
xfr query -v debug -s localhost -x indexes \
    -o remote_output.bed -m SRX012345 -i intervals.bed
```

Note that now `SRX012345` is not a file. Rather, it is a methylome
name, and should be available on the server. If the server can't find
the named methylome, it will respond indicating that methylome is not
available.
