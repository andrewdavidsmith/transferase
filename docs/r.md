# R API usage examples

I'm starting the R documentation here because I don't have time right now to
learn how to format the docs nicely within R (using Rd or roxygen2). I will
make proper R docs for this when I get the chance.

Note: I don't expect this to work at all on Windows.

## Installing the package

In R, packages are usually installed by building from source. There is a
section in the 'docs/building.md' file that explains how to build the R
package from a cloned repo. The R package itself is source, but not identical
to the cloned repo. So if you are trying to do this from the cloned repo
itself, it won't work. Assuming you have a file named
`transferase_0.5.0.tar.gz`, and it is the R package, you can install it within
R like this:

```R
install.packages("transferase_0.5.0.tar.gz", repos = NULL)
```

At the time of writing (mid-March 2025), the default GCC and R packages for
typical Linux distributions will not build the transferase R API. The current
Debian 'sid' branch, available through docker as 'debian:sid' can do the build
with default packages. See 'docs/building.md'. By late April 2025, the main
Ubuntu distribution should be able to build the transferase R API by default.

## Functions

The following functions are not associated with any particular object.

### Initialize logger

Initialize logging for transferase. This function is called automatically when
`library(transferase)` is called. You will never need to use it, and it takes
no arguments anyway.

Usage:
```R
init_logger()
```

Examples:
```R
init_logger()
```

### Configure transferase

The `config_xfr` function configures transferase, which includes creating a
configuration directory, writing configuration files, and typically
downloading metadata and configuration data corresponding to one or more
specified reference genomes. If you do this using the transferase command line
app, then you don't need to do it within R. But it must be done by some means
before making queries using transferase.

Usage:
```R
config_xfr(genomes, config_dir = "")
```

Arguments:

* `genomes`: A list of names of reference genome assemblies. These are in the
    format used by the UCSC Genome Browser (i.e., "hg38" instead of "GRCh38").
    There is no way to know if a typo has been made, as the system will simply
    assume an incorrect genome name is one that is not supported.

* `config_dir`: A directory to place the configuration. Default is
    `$HOME/.config/transferase`.

Examples:
```R
genomes <- c("hg38", "bosTau9")
config_dir <- "my_config_directory"
config_xfr(genomes, config_dir)
```

### Set transferase log level

The `set_xfr_log_level` function sets the log level for functions related to
transferase. Must be one of 'debug', 'info', 'error', 'warning' or
'critical'. The value 'debug' prints the most information, while 'critical'
could print nothing even if there is an error.

Usage:
```R
set_xfr_log_level(level)
```

Arguments:
* `level`: A string indicating the desired log level. One of 'debug', 'info',
    'error', 'warning' or 'critical'.

Examples:
```R
set_xfr_log_level("debug")
```

### Get transferase log level

Get the current log level, which is the default if you didn't use
`set_xfr_log_level`, or whatever log level you most recently set it to.

Usage:
```R
get_xfr_log_level()
```

Return value: A string indicating the log level.

Examples:
```R
get_xfr_log_level()
```

## MClient

The MClient class is one of two classes in the R API. MClient is an R6
class that serves as the interface for making remote transferase queries. You
will not be able to instantiate an object of MClient class until you have
either run the `config_xfr` function, or configured transferase using the
command line app (or did the same with the Python API).

### Data:

* `config_dir`: The name of the configuration directory associated with this
    MClient object. Again, this would have been configured using `config_xfr`
    or the command line transferase app.

* `data`: A pointer to the underlying object, which cannot be inspected
    directly. This object could be several hundred MB in size, depending on
    how many genomes you are working with.

### Constructor:

The constructor can take the name of a configuration directory, but most users
will leave this empty and allow it to use the default.

Usage:
```R
MClient$new(config_dir = "")
```

Arguments:
* `config_dir`: The name of a configuration directory; most users will
    typically leave this empty.

Examples:
```R
config_xfr(c("hg38", "mm39"), "some_directory")
client <- MClient$new("some_directory")
```

### do_query

This is the central function for querying methylomes using transferase.
Currently only "remote" queries are supported through the R package, but that
includes a server you run on your own machine (e.g., your laptop, in a
different window). This is explained in the usage docs for the transferase
command line app.

This function requires the names of methylomes to query, and a set of genomic
intervals that can be specified in different ways.

Usage:
```R
do_query(methylomes, query, genome = NULL, covered = FALSE)
```

Arguments:
* `methylomes`: A list of methylome names. These must be the names of
     methylomes that exist on the server. These will usually be SRA accession
     numbers, and the server will immediately reject any names that include
     letters other than `[a-zA-Z0-9_]`. Queries involving too many methylomes
     will be rejected; this number is roughly 45.

* `query`: This can be one of three types:
     - an integer value indicating a genomic bin size.
     - a data frame with genomic intervals
     - an MQuery object built from a set of genomic intervals.

* `genome`: The name of the reference genome corresponding to the given
    methylomes. This is only needed if the type for `query` is a data frame of
    genomic intervals.

* `covered`: A boolean value indicating whether the response to the query
    should include the number of covered sites for each interval.

Return value: A numeric matrix with one row for each query intervals, and
columns corresponding to the methylomes in the query. For each query
methylome, there will be columns named with suffixes `_M` and `_U` indicating
the number of methylated and unmethylated observations, respectively.
Additionally, if `covered = TRUE` was set, then there will be a third column,
indicated with the suffix `_C`, to indicate the number of covered CpG sites in
each intervals.

Examples:
```R
methylomes <- c("SRX3468816", "SRX3468835")

# Using genomic intervals
genome <- "hg38"
intervals <- read.table("refGene_hg38_promoters.bed")
levels <- client$do_query(methylomes, intervals, genome)

# Using a bin size
bin_size <- 100000
levels <- client$do_query(methylomes, bin_size)
```

### Format a transferase query

The `format_query` method of the MClient R6 class converts a data frame of
genomic intervals (chromosome, start, stop; half-open and zero-based) into a
MQuery object. The corresponding reference genome must be specified and that
reference genome must have been configured with the `config_xfr` command.

Usage:
```R
format_query(genome, intervals)
```

Arguments:
* `genome`: The name of reference genome assembly. This is in the format used
    by the UCSC Genome Browser (i.e., "hg38" instead of "GRCh38"). This genome
    must have been configured for transferase using the `config_xfr` function.

* `intervals`: A data frame with at least 3 columns, the first being character
    and the second and third being numeric. Each row is a genomic interval,
    with the first column giving the chromosome name, the second giving the
    start position and the third giving the stop position of the
    interval. These intervals are half-open and zero-based. The intervals need
    to be sorted by start position within each chromosome and chromosomes must
    be together, but the order of chromosomes does not matter.

Return value: an MQuery object corresponding to the given set of genomic
intervals and reference genome.

Examples:
```R
genome <- "hg38"
intervals <- read.table("refGene_hg38_promoters.bed")
client <- MClient$new()
query <- client$format_query(genome, intervals)
```

## MQuery

The MQuery class is the other R6 class in the transferase R API. The only
purpose for MQuery objects is to represent a set of genomic intervals that
will be used in repeated queries, because keeping the intervals in this form
makes each query faster. You can't do anything else with an MQuery object, and
you can't examine its internals.

There are methods associated with MQuery object that you should be using. The
only uses of MQuery objects is passing them to the `do_query` method of the
MClient class.

Examples:
```R
genome <- "hg38"
intervals <- read.table("refGene_hg38_promoters.bed")
client <- MClient$new()

# Create the MQuery object for the intervals
query <- client$format_query(genome, intervals)

# Now use the MQuery object
levels <- client$do_query(methylomes, query)
```
