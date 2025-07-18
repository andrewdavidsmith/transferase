# Transferase

- [What is transferase](#what-is-transferase)
- [What's new](#whats-new)
- [Current status](#current-status)
- [Installation](#installation)
- [Command line app](docs/command_line.md)
- [The Python package](docs/pyxfr.md)
- [The R package](docs/Rxfr.md)
- [All documentation](docs)

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
methylome database with roughly 14,000 vertebrate methylomes (as of
2025-06-25) that are whole-genome, sequencing-based and *high-quality*. Most
of these are split between human and mouse. The transferase clients include
tools to help identify which among those methylomes might be useful to inform
specific biological questions.

- If you work on Mac or Linux, there are lots of ways to get and use
  transferase.

- Transferase is in early development and is improving quickly.

- But this means frequent changes -- check back for updates, and star or watch
  this repo.

- I appreciate your feedback. You can open an issue on GitHub or contact me
  (Andrew) directly via the email in my GitHub profile.

## What's new

* **Important:** As of the release of v0.6.4, the format of future metadata
  will no longer work with previous versions of `xfr select`.

* **Important:** As of the release of v0.6.3, the server will not communicate
  with previous versions.

* Sliding window queries have been introduced in v0.6.3.

* Remote query speed has been improved, and for "bins" or "windows" queries
  this is by more than 3x.

* The transferase Python package, pyxfr, can now be installed using
  [pip](https://pypi.org/project/pyxfr/0.6.4) with Python >= 3.12 on Linux
  and macOS:
  ```console
  pip install pyxfr
  ```

## Current status

- Public server: It's open and currently serving over 14,300 high-quality
  methylomes. Right now (2025-07-17) the only restriction on use is that
  individual queries are limited to roughly 45 methylomes, and roughly 1M
  query intervals or bin/window size of 200bp. If you need more, do multiple
  queries.

- Linux: The binary installation for Linux should work on any Linux machine.

- macOS: The binary installation for macOS should work on any Mac running at
  least Ventura (macos-13; October 2022).

- Python: The python package, pyxfr, can be installed with `pip install pyxfr`
  and requires Python >= 3.12 but otherwise should work on any Linux and
  macOS-13 or later.

- R: Rxfr can be installed on Linux and macOS (R >= 4.4). Paste exactly this
  into your terminal:
  ```console
  (CXX23=$(R CMD config CXX23) && [ -n "$CXX23" ] && $CXX23 --version)
  ```
  If you get output and it says version 14.2 or greater, you can install Rxfr
  with devtools within R:
  ```console
  devtools::install_url('https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.4/Rxfr_0.6.4.tar.gz')
  ```
  Alternatives for both Linux and macOS are covered in
  [docs/installing_Rxfr.md](docs/installing_Rxfr.md).

## Installation

- **Linux**
  The Linux binary releases should work on any Linux system. You
  can find installers
  [here](https://github.com/andrewdavidsmith/transferase/releases/v0.6.4),
  including packages deb and rpm-based systems. The easiest is the
  [shell installer](https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.4/transferase-0.6.4-Linux.sh):
  ```console
  sh transferase-0.6.4-Linux.sh --prefix=/desired/install/location
  /desired/install/location/bin/xfr  # check that it worked
  ```

- **Mac**
  The transferase binary for Mac is a "universal binary" and
  should work on any Mac. It is built to work on Ventura or later, but it has
  worked on a much older system. The easiest is the
  [shell installer](https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.4/transferase-0.6.4-macOS.sh):
  ```console
  sh transferase-0.6.4-macOS.sh --prefix=/desired/install/location
  /desired/install/location/bin/xfr  # check that it worked
  ```

- **Python**
  pyxfr can be installed using
  [pip](https://pypi.org/project/pyxfr/0.6.4) with Python (>= 3.12) on Linux
  and macOS:
  ```console
  pip install pyxfr
  python3 -c "from pyxfr import pyxfr; help(pyxfr)"  # check that it worked
  ```

- **R**
  Detailed instructions to install Rxfr are in
  [docs/installing_Rxfr.md](docs/installing_Rxfr.md).

- **Source** Detailed instructions for building from source are in
  [docs/building.md](docs/building.md).

- **Linux packages**
  ```console
  # Red Hat or Fedora (not sure SUSE has dnf)
  rpm -i transferase-0.6.4-Linux.rpm              # See what will be installed
  sudo dnf install ./transferase-0.6.4-Linux.rpm  # Install (note the dot-slash)
  dnf info transferase                            # See what was installed
  sudo dnf remove transferase                     # Uninstall

  # Ubuntu or Debian
  dpkg --info transferase-0.6.4-Linux.deb         # See what will be installed
  sudo apt install ./transferase-0.6.4-Linux.deb  # Install (note the dot-slash)
  apt info transferase                            # See what was installed
  sudo apt remove transferase                     # Uninstall
  ```
