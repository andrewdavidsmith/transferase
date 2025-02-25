# Transferase

The transferase system for retrieving methylome data from methbase.

The MethBase2 database currently has well over 13,000 high-quality methylomes
for vertebrate species, mostly split equally between human and mouse. For most
epigenomics scientists, answering important questions requires identifying
relevant data sets and obtaining methylation levels summarized at relavant
parts of the genome.  Transferase provides fast access to MethBase2 via
queries made up of two parts: a set of genomic intervals and one or more
methylomes of interest. Queries return methylation levels in each interval for
each specified methylome. There is no need to download entire methylomes
because the server can quickly respond with the methylation levels in query
regions -- tens of thousands of intervals in a single query, with responses as
fast as a few seconds over regular home internet.  In some situations the
transferse server can respond faster than you could load the data files if had
them locally on your laptop or workstation.

- As of v0.5.0, the server open to the public. I don't yet know how well the
  server can stand up to heavy traffic -- even from a small number of users
  with big queries. So I might need to put restrictions on the sizes of
  queries. As of now, it's quite open.

- The current version (v0.5.0) is still in an early development stage,
  although many improvements have been made. It works, but hasn't seen enough
  use to know if there are any serious bugs lurking.

- I appreciate feedback. You can open an issue on github or contact me
  (Andrew) directly.

- If you have a lot of methylome data, whether it's from whole-genome
  bisulfite sequencing, or from Nanopore, you can use transferase locally to
  do very fast data analysis.

- Target systems are Mac and Linux. As of v0.5.0, there are binary releases
  that should work on any Linux and any Mac machine.

- Target platforms are command line, Python and R. The R API is not ready yet.

- The current release has binaries that should work on almost any Linux
  machine. The Python package needs Python 3.13, but otherwise will run on
  almost any Linux machine.

## Documentation

The instructions for using transferase on the command line are in the
`docs/command_line.md` file and very basic docs for the Python API are in
`docs/python.md`.

## Installing transferase

Instructions for installing from source have been removed from this flie. It's
become a complicated process. The best documentation will be to first make the
Dockerfile for the build system available. This will happen soon. After that
I'll explain the build process once it has stabilized.

### Linux

You can find all the available installers from "releases" page (assuming you
are reading this on github.com).  You can download a shell installer
[here](https://github.com/andrewdavidsmith/transferase/releases/download/v0.5.0/transferase-0.5.0-Linux.sh).
Then run the downloaded installer (likely you want to first install it beneath
your home dir):

```console
sh transferase-0.5.0-Linux.sh --prefix=${PREFIX}
```

This will prompt you to accept the license, and then it will install the
transferase binaries in `${PREFIX}/bin`, along with some config files in
`${PREFIX}/share`. If you want to install it system-wide, and have admin
privileges, you can do:

```console
sh transferase-0.5.0-Linux.sh --prefix=/usr/local
```

If you are on Debian or Ubuntu, and have admin privileges, you can use the
Debian package and then transferase will be tracked in the package management
system. Similarly, there is an RPM for Linux distributions that use those. You
can get these from the "releases" page. For Ubuntu and Debian, install like
this:

```console
sudo dpkg -i ./transferase-0.5.0-Linux.deb
```

For Red Hat, Fedora or SUSE, install like this:

```console
sudo rpm -i ./transferase-0.5.0-Linux.rpm
```

### Mac

For Mac two architectures are supported: the older "x86_64" and the the arm64
("Apple Silicon"). If you are using an older Mac, then the binaries built on
macOS 13 should work for you. If you are using a newer Mac, then the binaries
built on macOS 14 or 15 should work for you. I think this should be true
regardless of which version of macOS you are actually using -- as long as you
have the right hardware architecture. However, I can't test all systems in all
combinations, so feedback is appreciated.

Curently only a shell installer is available. You can download the installer
matching your version at the releases page on github. Assuming it's macos-15
and ARM64, then do this:

```
sh transferase-0.5.0-Linux.sh --prefix=${PREFIX}
```

This will prompt you to accept the license, and then it will install the
transferase binaries in `${PREFIX}/bin`, along with some config files in
`${PREFIX}/share`. For v0.5.0, be careful with the bash completions file on
macos -- it's a bit unstable.

## Installing the Python API

Transferase has a Python API that allows the same queries to be done within
Python. Almost all "client side" behaviors of transferase are available
through the Python API. Installation should be easy. You will need Python
3.13, which is the latest version as of transferase v0.5.0. Download the wheel
file (`*.whl`) that matches your system.  On Linux, there is only one and it
should work on any Linux system going back many years. On macOS, there are
three (see above for how to select). Assuming you have the Linux wheel, just
download it into your current directory and do this:

(These instructions assume installation in a virtual environment because it
avoids any risk if something goes wrong.)

```console
python3 -m venv .venv
. .venv/bin/activate
pip install transferase-0.5.0-cp312-none-manylinux_2_17_x86_64.whl
# To test; if you see output, it worked
python3 -c "import transferase; help(transferase)"
```

If you happen to get the wrong version, pip will probably tell you it's not
compatible, so just try another. If none work, let me know.
