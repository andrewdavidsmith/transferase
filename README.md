# Transferase

[**What's New**](#whats-new)

## What is transferase?

Transferase is a system for retrieving DNA methylation levels through
user-defined sets of genomic intervals. When analyzing whole-genome DNA
methylation profiles ("methylomes"), this form of summary statistic is central
to both hypothesis testing and exploratory data analysis. In particular, it is
the basis of identifying differentially methylated regions, clustering
methylomes based on subsets of genes and other genomic features, and
clustering genomic features according to their methylation dynamics.

The motivation behind transferase is simple. Methylomes derived from
whole-genome sequencing are huge. Even with access to high-performance
computing, downloading, moving and processing a collection of methylomes takes
time. Any study leveraging this type of data will examine multiple different
sets of genomic features and explore a variety of contrasts between
methylomes. This process can and should be faster and more convenient. But
attaining these goals requires methods that synergize to exploit certain
unique characteristics of sequencing-based whole-genome DNA methylation
profiles.

Transferase is a complete system: it includes server software, a collection of
file formats and algorithms, client apps and APIs, all co-designed to maximize
efficiency of data retrieval. Transferase can run on a local system for an
individual user, or remotely granting more users access to larger volumes of
data. With a good network connection, queries to a remote transferase server
can be faster than similar analysis of locally stored files.

A public transferase server currently provides access to MethBase2, a
methylome database with roughly 13,500 vertebrate methylomes that are
whole-genome, sequencing-based and *high-quality*. Most of these are split
between human and mouse. The transferase clients include tools to help
identify which among those methylomes might be useful to inform specific
biological questions.

- If you work on Mac or Linux, there are lots of ways to get and use
  transferase.

- Transferase is in early development and is improving quickly.

- But this means frequent changes -- check back for updates, and star or watch
  this repo.

- I appreciate your feedback. You can open an issue on GitHub or contact me
  (Andrew) directly via the email in my GitHub profile.

- A table-of-contents for the documentation should be "here" but I don't have
  it organized yet. All current documentation can be found in the [docs](docs)
  directory of this repo.

## What's new

* The options for query output formats and customization have been
  significantly improved in v0.6.2.

* The `select` command now allows users to make methylome groups as they
  select them methylomes they want to query.

* The `select` command in the transferase command line app now includes more
  details about available methylomes (hit 'd' to turn on details).

* The transferase Python package, pyxfr, can now be installed using
  [pip](https://pypi.org/project/pyxfr/0.6.1) with Python >= 3.12 on Linux
  and macOS (at least Ventura):
  ```console
  pip install pyxfr
  ```

* The Python and R packages each include a function that loads a data frame
  with extra information about the methylomes available through the public
  transferase server.

## Current status

- Public server: It's open and currently serving a lot of high-quality
  methylomes. Right now (2025-05-10) the only restriction on use is that
  individual queries are limited to roughly 45 methylomes, and roughly 1M
  query intervals. If you need more, just do multiple queries.

- Linux: The binary installation for Linux should work on any Linux machine
  going back 15 years.

- macOS: The binary installation for macOS should work on any Mac hardware
  that is running at least Ventura (macos-13; October 2022).

- Python: The python package, pyxfr, can be installed with `pip install pyxfr`
  and requires Python >= 3.12 but otherwise should work on any Linux and any
  Mac running at least Ventura.

- R: The R package, Rxfr, can be installed on Linux and macOS, but it needs R
  version >= 4.4 and is not yet easy to install. Popular Linux distributions,
  and macOS 15, now include the right compilers. When the maintainers of R
  update some of their configurations, installing Rxfr will become simpler.

## Documentation

The instructions for using transferase on the command line are in
[docs/command_line.md](docs/command_line.md). Instructions for setting up a
server are in [docs/server.md](docs/server.md). Basic usage examples for
pyxfr are in [docs/python.md](docs/python.md). There is also built-in
documentation in Python and R (use `help(pyxfr)` in Python or
`library(help=Rxfr)` in R).

## Installation

### Linux

The binary releases for Linux should work on any Linux system. You can find
the installers
[here](https://github.com/andrewdavidsmith/transferase/releases/v0.6.2). If
you aren't familiar with installing command line tools, try using the [shell
installer](https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.2/transferase-0.6.2-Linux.sh)
like this:

```console
sh transferase-0.6.2-Linux.sh --prefix=/desired/install/location
```

If you run this and see output, it worked:

```console
/desired/install/location/bin/xfr
```

To remove it just delete the installation directory:

```console
rm -r /desired/install/location
```

Packages are available for Linux: a
[deb](https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.2/transferase-0.6.2-Linux.deb)
for Ubuntu or Debian and an
[rpm](https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.2/transferase-0.6.2-Linux.rpm)
for Red Hat, Fedora or SUSE. If you plan to install system-wide, using the
package managers is a very good idea. More [here](#Linux-package-managers).

### Mac

The transferase binary release for Mac is a "universal binary" which should
work on any Mac. It is built to work on Ventura or later, but it has worked on
a much older system. You can find the installers
[here](https://github.com/andrewdavidsmith/transferase/releases/v0.6.2). If
you aren't familiar with installing command line tools, try using the [shell
installer](https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.2/transferase-0.6.2-macOS.sh)
like this:

```console
sh transferase-0.6.2-macOS.sh --prefix=/desired/install/location
```

Then check that it worked like this:

```console
/desired/install/location/bin/xfr
```

And to remove it just delete the installation directory:

```console
rm -r /desired/install/location
```

### The Python package

The Python package is named pyxfr, and it can be installed using
[pip](https://pypi.org/project/pyxfr/0.6.2) with Python >= 3.12 on Linux and
macOS (at least Ventura):

```console
# Use a virtual environment
python3 -m venv .venv
. .venv/bin/activate
pip install pyxfr
# Check that it worked
python3 -c "from pyxfr import pyxfr; help(pyxfr)"
```

### The R package

The R package is named Rxfr. Please see the instructions in
[docs/installing_Rxfr.md](docs/installing_Rxfr.md).

### Building from source

Detailed instructions for building transferase from source can be found in
[docs/building.md](docs/building.md).

### Linux package managers

Note that the "install" steps below might need some indication that the
package filename is a file and not the name of some remote resource. That's
why I prepended the dot-slash below.

```console
# Red Hat or Fedora (not sure SUSE has dnf)
rpm -i transferase-0.6.2-Linux.rpm              # See what will be installed
sudo dnf install ./transferase-0.6.2-Linux.rpm  # Install
dnf info transferase                            # See what was installed
sudo dnf remove transferase                     # Uninstall

# Ubuntu or Debian
dpkg --info transferase-0.6.2-Linux.deb         # See what will be installed
sudo apt install ./transferase-0.6.2-Linux.deb  # Install
apt info transferase                            # See what was installed
sudo apt remove transferase                     # Uninstall
```
