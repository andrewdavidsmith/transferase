# Installing the transferase R package

The transferase R package is named Rxfr. This file has multiple ways to do the
installation. Hopefull I can get this on CRAN and make it much easier. All of
these instructions assume you are using R version 4.4 or greater. It will not
work on earlier versions of R.

## Installing on Debian

Here I'm using Debian "sid" which is ahead of the stable release (Note: also
tested and working with Ubuntu "plucky"). These instructions should work on
latest stable Ubuntu or Debian by late April 2025.

Assuming a clean Debian, get the dependencies:

```console
apt-get update && \
DEBIAN_FRONTEND=noninteractive \
apt-get install -y --no-install-recommends \
    r-base \
    make \
    g++ \
    libssl-dev \
    zlib1g-dev
```

You could also install `r-base-dev` instead of `r-base`, and skip the others
except `libssl-dev`, but it would result in a larger total install, and isn't
needed.

Then get the R dependencies. We do that separately because the auto-install of
dependencies is annoying when installing our own package from a local archive:

```console
R -e "install.packages(c('Rcpp', 'R6'), repos = 'https://cloud.r-project.org')"
```

Get the Rxfr source package:

```console
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.0/Rxfr_0.6.0.tar.gz
```

And install it like this:

```console
R CMD INSTALL Rxfr_0.6.0.tar.gz
```

Or within R if you prefer:

```console
install.packages("Rxfr_0.6.0.tar.gz")
```

## Installing with Conda

### Linux

In these instructions I use `mamba` instead of `conda`, and I use it through
`miniforge3`. On a fresh install of Ubuntu (docker `ubuntu:latest`), we need
the following, but likely you already have these:

```console
apt-get update && apt-get install -y ca-certificates wget
```

Then we can get Minforge like this:

```console
wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-x86_64.sh
```

And install it like this (it will ask questions):

```console
sh Miniforge3-Linux-x86_64.sh
```

To install R through Conda:

```console
mamba install conda-forge::r-base
```

Fortunately, as of late April 2025, for most "latest" releases of Linux the
Conda R package will have a compiler that can build Rxfr. To install the R
packages that Rxfr needs:

```console
R -e "install.packages(c('Rcpp', 'R6'), repos = 'https://cloud.r-project.org')"
```

Then we can download the Rxfr sources like this:

```console
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.0/Rxfr_0.6.0.tar.gz
```

Now we just need to build and install Rxfr. If you can, use more cores by
setting `MAKEFLAGS`.

```console
MAKEFLAGS="-j32" R CMD INSTALL Rxfr_0.6.0.tar.gz
```

### macOS

This method only works on macOS-15 and later, because the compiler that R uses
when obtained through Conda on macOS is the native Apple Clang compiler, and
earlier versions are unable to build Rxfr (they lag behind in support for
standards). In addition, we might need to help R and the compiler interact.

Get Miniforge like this:

```console
wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-MacOSX-arm64.sh
```

Install it like this, and pay attention to what it asks you to do:

```console
sh Miniforge3-Linux-x86_64.sh
```

Install R through Conda like this:

```console
mamba install conda-forge::r-base
```

Fortunately, as of late April 2025, for most "latest" releases of Linux the
Conda R package will have a compiler that can build Rxfr. To install the R
packages that Rxfr needs:

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
configuration for tweaking the compiler settings used by R. I've been
successful just copying the existing configuration for C++17, and changing the
17 to 23 where it matters. The commands below will make your `~/R` directory
if it doesn't exist, then extract R's information about C++17, change the "17"
to "23", and append it to your personal `Makevars` file:

```console
mkdir -p ${HOME}/.R && \
R CMD config --all | grep CXX17 | \
    sed "s/CXX17/CXX23/g; s/++17/++23/g" >> ${HOME}/.R/Makevars
```

If you already had a `Makevars` file, this won't overwrite whatever was in it,
and if you had already done setup for C++23, you are probably way ahead of me.

Now we can proceed. These two commands install the R dependencies for Rxfr and
then get the source package:

```console
R -e "install.packages(c('Rcpp', 'R6'), repos = 'https://cloud.r-project.org')"
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.0/Rxfr_0.6.0.tar.gz
```

The final step is just to do the install (using `MAKEFLAGS` to speed up the
process with more cores):

```console
MAKEFLAGS="-j16" R CMD INSTALL Rxfr_0.6.0.tar.gz
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
wget https://github.com/andrewdavidsmith/transferase/releases/download/v0.6.0/Rxfr_0.6.0.tar.gz
```

The final step is just to do the install (using `MAKEFLAGS` to speed up the
process with more cores):

```console
MAKEFLAGS="-j16" R CMD INSTALL Rxfr_0.6.0.tar.gz
```

If you get any errors related to OpenSSL, the fix that works for me is to put
the installation location for OpenSSL in the `$HOME/.R/Makevars` file. You
will need to open `$HOME/.R/Makevars` with a text editor. Then add these
lines:

```console
PKG_CPPFLAGS += -I$(shell brew --prefix openssl)/include
PKG_LIBS += -L$(shell brew --prefix openssl)/lib
```
