# Installing the transferase R package

This file explains multiple ways to install Rxfr. Hopefull I can get Rxfr on
CRAN or R-universe and make it much easier. All of these instructions assume
you are using R version 4.4 or greater. It will not work on earlier versions
of R.

## Linux using system packages

The sequence of commands below can be copied and pasted into a terminal for
the install. Distributions are listed above the commands that do the install.
In each case, setting a larger value for `MAKEFLAGS="-j1"` will speedup the
install.

- Ubuntu 25.04 (Plucky Puffin): `docker pull ubuntu:plucky`
- Debian 13 (Trixie): `docker pull debian:trixie`

```console
export MAKEFLAGS="-j1" DEBIAN_FRONTEND=noninteractive &&
apt-get update &&
apt-get install -y --no-install-recommends \
    r-base \
    r-base-dev \
    r-cran-devtools \
    libssl-dev && \
R -e "library(devtools); \
devtools::install_url('https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.2/Rxfr_0.6.2.tar.gz')"
```

- Linux Mint 22 (Wilma): `docker pull linuxmintd/mint22-amd64:latest`

```console
export MAKEFLAGS="-j1" DEBIAN_FRONTEND=noninteractive && \
apt-get update && \
apt-get install -y --no-install-recommends \
    r-base \
    make \
    g++ \
    g++-14 \
    zlib1g-dev \
    wget \
    libssl-dev && \
R -e "install.packages(c('Rcpp', 'R6'), repos = 'https://cloud.r-project.org')" && \
mkdir -p ${HOME}/.R && \
R CMD config --all | grep CXX17 | \
    sed "s/CXX17/CXX23/g; s/++17/++23/g" | \
    sed "s/^CXX23 = g++/CXX23 = g++-14/" >> ${HOME}/.R/Makevars && \
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.2/Rxfr_0.6.2.tar.gz && \
R CMD INSTALL Rxfr_0.6.2.tar.gz
```

- Fedora 42: `docker pull fedora:42`

```console
# --setopt=tsflags= silences a warning
export MAKEFLAGS="-j1" && \
dnf update -y && \
dnf install -y \
    openssl-devel \
    R-core \
    R-core-devel \
    R-devtools \
    --setopt=tsflags= && \
R -e "library(devtools); \
devtools::install_url('https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.2/Rxfr_0.6.2.tar.gz')"
```

## Linux using Conda

This method can be done by users without the privileges to use `apt` on their
system, but `wget` must be available. These commands below have been tested
and work on:

- Ubuntu 24.04 (Noble Numbat) `docker pull ubuntu:latest`
- Debian 12 (Bookworm) `docker pull debian:latest`
- Linux Mint 22 (Wilma): `docker pull linuxmintd/mint22-amd64:latest`

If you are running the commands below in a docker container, first do:

```console
export DEBIAN_FRONTEND=noninteractive && \
apt-get update && \
apt-get install -y --no-install-recommends ca-certificates wget
```

These commands will create a new Miniforge tree in `$HOME/Rxfr_miniforge3` and
install the Rxfr package in that location:

```console
export MAKEFLAGS="-j1" && \
wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-x86_64.sh && \
sh Miniforge3-Linux-x86_64.sh -bsup ${HOME}/Rxfr_miniforge3 && \
export PATH=${HOME}/Rxfr_miniforge3/bin:$PATH && \
conda install -y conda-forge::r-base conda-forge::zlib && \
mkdir -p ${HOME}/.R && \
R CMD config --all | grep CXX17 | \
    sed "s/CXX17/CXX23/g; s/++17/++23/g" >> ${HOME}/.R/Makevars && \
R -e "install.packages(c('Rcpp', 'R6'), repos = 'https://cloud.r-project.org')" && \
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.2/Rxfr_0.6.2.tar.gz && \
R CMD INSTALL Rxfr_0.6.2.tar.gz
```

## macOS using Conda

This method has only been tested on macOS-15 and the commands below assume
arm64 hardware. It assumes `wget` is already available.

```console
export MAKEFLAGS="-j1" && \
wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-MacOSX-arm64.sh && \
sh Miniforge3-MacOSX-arm64.sh -bsup ${HOME}/Rxfr_miniforge3 && \
export PATH=${HOME}/Rxfr_miniforge3/bin:$PATH && \
conda install -y conda-forge::r-base conda-forge::zlib && \
mkdir -p ${HOME}/.R && \
R CMD config --all | grep CXX17 | \
    sed "s/CXX17/CXX23/g; s/++17/++23/g" >> ${HOME}/.R/Makevars && \
R -e "install.packages(c('Rcpp', 'R6'), repos = 'https://cloud.r-project.org')" && \
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.2/Rxfr_0.6.2.tar.gz && \
R CMD INSTALL Rxfr_0.6.2.tar.gz
```

## macOS using Homebrew

This method also has only been tested on macOS-15. Assuming a fresh macOS that
has [Homebrew](https://brew.sh) already installed, use these commands:

```console
export MAKEFLAGS="-j1" && \
brew update && \
brew install openssl && \
brew install --cask r && \
mkdir -p ${HOME}/.R && \
R CMD config --all | grep CXX17 | \
    sed "s/CXX17/CXX23/g; s/++17/++23/g" >> ${HOME}/.R/Makevars && \
R -e "install.packages(c('Rcpp', 'R6'), repos = 'https://cloud.r-project.org')" && \
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.2/Rxfr_0.6.2.tar.gz && \
R CMD INSTALL Rxfr_0.6.2.tar.gz
```

The above commands work in a clean macos-15 image from GitHub. I had errors on
my machine related to OpenSSL, and the fix that worked for me was to put the
installation location for OpenSSL in the `$HOME/.R/Makevars` file. If you need
this fix, open `$HOME/.R/Makevars` with a text editor and add these lines:

```console
PKG_CPPFLAGS += -I$(shell brew --prefix openssl)/include
PKG_LIBS += -L$(shell brew --prefix openssl)/lib
```
