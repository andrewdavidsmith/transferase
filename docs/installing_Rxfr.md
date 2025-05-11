# Installing the transferase R package

The transferase R package is named Rxfr. This file explains multiple ways to
do the installation. Hopefull I can get Rxfr on CRAN or R-universe and make it
much easier. All of these instructions assume you are using R version 4.4 or
greater. It will not work on earlier versions of R.

## Installing on Linux

The following instructions work on both Debian "sid" and Ubuntu "plucky", both
of which are ahead of the stable releases. These instructions should work on
latest stable Ubuntu or Debian by late April 2025.

### Using devtools

This approach has fewer commands, but might take longer if you have a slow
network due to the size of the dependencies. We get the in Ubuntu/Debian like
this:

```console
export DEBIAN_FRONTEND=noninteractive &&
apt-get update &&
apt-get install -y --no-install-recommends \
    r-base \
    r-base-dev \
    r-cran-devtools \
    libssl-dev
```

Then, from inside R, use these commands (replace 0.6.2 with any more recent
version):

```R
library(devtools)
devtools::install_url("https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.2/Rxfr_0.6.2.tar.gz")
```

### More packages but faster

This method has more packages to install but is faster:

```console
export DEBIAN_FRONTEND=noninteractive && \
apt-get update && \
apt-get install -y --no-install-recommends \
    r-base \
    make \
    g++ \
    zlib1g-dev \
    wget \
    libssl-dev
```

Then get the R dependencies. We do that separately because the auto-install of
dependencies isn't very convenient when installing our own package from a
local file (which we will). Also, this needs to be done before installing:

```console
R -e "install.packages(c('Rcpp', 'R6'), repos = 'https://cloud.r-project.org')"
```

Get the Rxfr source package:

```console
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.2/Rxfr_0.6.2.tar.gz
```

And install it like this:

```console
R CMD INSTALL Rxfr_0.6.2.tar.gz
```

Or within R if you prefer:

```R
install.packages("Rxfr_0.6.2.tar.gz")
```

## Installing with Conda

### Linux

These instructions have been tested and work on the "ubuntu:latest" image. In
these instructions I use `mamba` instead of `conda`, and I use it through
`miniforge3`. On a fresh install of Ubuntu (docker `ubuntu:latest`), we need
the following, but likely you already have these:

```console
apt-get update && \
apt-get install -y ca-certificates wget
```

Then we can download Minforge and install it like this (it will ask some questions):

```console
wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-x86_64.sh &&
sh Miniforge3-Linux-x86_64.sh
```

To install R through Conda (along with ZLib, which we need):

```console
mamba install -y conda-forge::r-base conda-forge::zlib
```

Next do this:

```console
R CMD config CXX23
```

If you got any output, then you can skip the next step. Otherwise we need to
make sure that R (through Conda) knows the compiler it will try to use can do
C++23. Fortunately, as of late April 2025, for most "latest" releases of Linux
the Conda R package have a compiler that can build Rxfr. I've been successful
just copying the existing configuration for C++17, and changing the "17" to
"23" where it matters. The commands below will make your `~/R` directory if it
doesn't exist, then setup some values for C++23 in your personal `Makevars`
file:

```console
mkdir -p ${HOME}/.R && \
R CMD config --all | grep CXX17 | \
    sed "s/CXX17/CXX23/g; s/++17/++23/g" >> ${HOME}/.R/Makevars
```

If you already had a `Makevars` file, this won't overwrite whatever was in it.

To install the R packages that Rxfr needs:

```console
R -e "install.packages(c('Rcpp', 'R6'), repos = 'https://cloud.r-project.org')"
```

Then we can download the Rxfr sources like this:

```console
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.2/Rxfr_0.6.2.tar.gz
```

Now we just need to build and install Rxfr. If you can, use more cores by
setting `MAKEFLAGS`.

```console
MAKEFLAGS="-j32" R CMD INSTALL Rxfr_0.6.2.tar.gz
```

### macOS

This method only works on macOS-15 (Sequoia) and later, because the compiler
that R uses when obtained through Conda on macOS is the native Apple Clang
compiler, and earlier versions are unable to build Rxfr (they lag behind in
support for standards). In addition, we might need to help R and the compiler
interact.

If you don't already have conda or mamba installed, get Miniforge like this
and then install it:

```console
wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-MacOSX-arm64.sh && \
sh Miniforge3-Linux-x86_64.sh
```

The installation asks questions. Pay attention at the end because it will tell
you how to activate it.

Install R through Conda like this, along with zlib:

```console
mamba install -y conda-forge::r-base conda-forge::zlib
```

To install the R packages that Rxfr needs:

```console
R -e "install.packages(c('Rcpp', 'R6'), repos = 'https://cloud.r-project.org')"
```

Next do this:

```console
R CMD config CXX23
```

If you got any output, then you can skip the next step. Otherwise we need to
make sure that R (through Conda) knows the standard compiler with macOS-15 can
do C++23. We will put some info in `$HOME/.R/Makevars`, which is your personal
configuration for tweaking the compiler settings used by R (this was explained
above for Linux, search for the "C++23"). We will use the same method of
copying the config for C++17 and changing the "17" to "23":

```console
mkdir -p ${HOME}/.R && \
R CMD config --all | grep CXX17 | \
    sed "s/CXX17/CXX23/g; s/++17/++23/g" >> ${HOME}/.R/Makevars
```

Now we can proceed. These two commands install the R dependencies for Rxfr and
then get the source package:

```console
R -e "install.packages(c('Rcpp', 'R6'), repos = 'https://cloud.r-project.org')"
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.2/Rxfr_0.6.2.tar.gz
```

The final step is just to do the install (using `MAKEFLAGS` to speed up the
process with more cores):

```console
MAKEFLAGS="-j16" R CMD INSTALL Rxfr_0.6.2.tar.gz
```

## Installing on macOS with Homebrew

I can only confirm this works using macOS-15 or greater. Assuming a fresh
macOS that has [Homebrew](https://brew.sh) already installed, start like this:

```console
brew update && \
brew install openssl && \
brew install --cask r
```

OpenSSL is needed for web-related things, and Homebrew doesn't always put
things in the most convenient place. If this causes problems, I will try to
address them below.

And we check that the compiler R sees can handle C++23 with this command.

```console
R CMD config CXX23
```

If the above command gave no output, we need to do a same steps as with Conda,
which I repeat here. We will need to tell R (as configured through Homebrew)
that our compiler can do C++23. If you are fully updated with macOS-15, then
this should be true. We will put some info in `$HOME/.R/Makevars`, your
personal configuration file for tweaking compiler settings used by R.  The
method I use is to just grab the configuration for C++17, and replace the "17"
with "23". These commands will do that:

```console
mkdir -p ${HOME}/.R && \
R CMD config --all | grep CXX17 | \
    sed "s/CXX17/CXX23/g; s/++17/++23/g" >> ${HOME}/.R/Makevars
```

If you already had a `Makevars` file, this won't overwrite whatever was in it,
and if you had already done setup for C++23, you probably know it.

The next steps are the same as with Conda: install the R dependencies for Rxfr
and then get the Rxfr source package:

```console
R -e "install.packages(c('Rcpp', 'R6'), repos = 'https://cloud.r-project.org')"
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.2/Rxfr_0.6.2.tar.gz
```

The final step is just to do the install (using `MAKEFLAGS` to speed up the
process with more cores):

```console
MAKEFLAGS="-j16" R CMD INSTALL Rxfr_0.6.2.tar.gz
```

If you get any errors related to OpenSSL, the fix that works for me is to put
the installation location for OpenSSL in the `$HOME/.R/Makevars` file. You
will need to open `$HOME/.R/Makevars` with a text editor. Then add these
lines:

```console
PKG_CPPFLAGS += -I$(shell brew --prefix openssl)/include
PKG_LIBS += -L$(shell brew --prefix openssl)/lib
```
