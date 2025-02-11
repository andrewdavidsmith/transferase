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

- The current version (v0.4.0) is still in an early development stage.
  It works, but hasn't seen enough use to know if there are any
  serious bugs lurking.

- As of v0.4.0, the server is not yet open to the public. Hopefully it
  will be available by sometime in the first half of February 2025.

- If you want to help test transferase, also get to use it early,
  contact me (Andrew) and I can will give you early access.

- If you have a lot of methylome data, whether it's from whole-genome
  bisulfite sequencing, or from Nanopore, you can use transferase
  locally to do very fast data analysis.

- Target systems are Mac and Linux. Currently a binary release is only
  available for Linux and x86 Mac, but one should follow soon for
  newer Macs.

- Target platforms are command line, Python and R. Currently the
  command line tools work, and the Python API also works, but with
  limited testing.

- The current release has binaries that should work on almost any
  Linux machine. The Python package needs Python 3.12, but otherwise
  will run on almost any Linux machine.

## Documentation

The instructions for using transferase on the command line are in the
`docs/command_line.md` file and very basic docs for the Python API are
in `docs/python.md`.

## Installing transferase

Instructions for installing from source have been removed from this
flie. It's become a complicated process. The best documentation will
be to first make the Dockerfile for the build system available. This
will happen soon. After that I'll explain the build process once it
has stabilized.

### Install pre-compiled binaries

You can find all the available installers from "releases" page
(assuming you are reading this on github.com). These instructions
assume Linux. You can download a shell installer
[here](https://github.com/andrewdavidsmith/transferase/releases/download/v0.4.0/transferase-0.4.0-Linux.sh).
Then run the downloaded installer (likely you want to first install it
beneath your home dir):
```console
sh transferase-0.4.0-Linux.sh --prefix=${PREFIX}
```

This will prompt you to accept the license, and then it will install
the transferase binaries in `${PREFIX}/bin`, along with some config
files in `${PREFIX}/share`. If you want to install it system-wide, and
have admin privileges, you can do:
```console
sh transferase-0.4.0-Linux.sh --prefix=/usr/local
```

If you are on Debian or Ubuntu, and have admin privileges, you can use
the Debian package and then transferase will be tracked in the package
management system. Similarly, there is an RPM for Linux distributions
that use those. You can get these from the "releases" page. For Ubuntu
and Debian, install like this:
```console
sudo dpkg -i ./transferase-0.4.0-Linux.deb
```
For Red Hat, Fedora or SUSE, install like this:
```console
sudo rpm -i ./transferase-0.4.0-Linux.rpm
```
