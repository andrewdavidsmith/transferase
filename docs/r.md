# R API usage examples

I'm still putting R documentation here. I now have most of it in the sources
and formatted into Rd using roxygen2. So it should appear using
`? function_name` within R.

Note: I don't expect this to work at all on Windows.

## Installing the package

In R, packages are usually installed by building from source. The file
[`docs/installing_Rxfr.md`](docs/installing_Rxfr.md) has instructions for two
ways to install the R package on Linux, and two ways to do it on macOS. These
are "source" installations, but the R package archive
[`Rxfr_0.6.1.tar.gz`](releases/download/v0.6.1/Rxfr_0.6.1.tar.gz) you would be
installing is not the same as the source you would get from a clone of the
repo. If you really want to start from the cloned repo, the instructions are
in [`docs/building.md`](docs/building.md). If you have a file named
`Rxfr_0.6.1.tar.gz`, and if the stars are perfectly aligned, you can install
it within R like this:

```R
install.packages("Rxfr_0.6.1.tar.gz")
```

Likely that won't work, so see
[`docs/installing_Rxfr.md`](docs/installing_Rxfr.md).

## Functions

### Load transferase metadata

The `load_methbase_metadata` function will return a data frame with metadata
about the methylomes you can query using transferase. These are the
high-quality methylomes in MethBase2. The `load_methbase_metadata` function
needs the name of a reference genome (the assembly, using UCSC Genome Browser
names, like hg38).

Usage:
```R
config_xfr(genomes, config_dir = "")
```

Arguments:

* `genome`: The name of a reference genome assembly.

* `config_dir`: A directory where the Rxfr has been configured. Most often
    this is left empty, and the default is `$HOME/.config/transferase`.

Details:

This information is available for each methylome:

- `study`: The SRA study accession for the corresponding study.
- `experiment`: The SRA experiment accession, and the identifier for the methylome.
- `assembly`: This is the genome. I might rename this to `genome` soon.
- `bsrate`: Estimated bisulfite conversion rate.
- `meth`: Estimated global weighted mean methylation level.
- `depth`: The average number of reads covering each CpG site.
- `mapping`: The fraction of *uniquely* mapping reads. Can be far less than
  the fraction that map.
- `uniq`: The fraction of uniquely mapping reads that are not duplicates.
- `sites`: The fraction of CpG sites covered by at least one read.
- `pmd_count`: Number of PMDs identified in the methylome. Note: this number
  will be low, but rarely zero, for methylomes that do not exhibit PMDs at
  all. If the fraction of the genome covered by PMDs (`pmd_count` x
  `pmd_size`) is less than 10 percent, any identified PMDs are sure
  false-positives.
- `pmd_size`: The mean size of identified PMDs.
- `hmr_count`: Number of identified HMRs. If this number is much more than
  100K, then many are likely false-positives.
- `hmr_size`: The mean size of identified HMRs. If these are too large, then
  they could easily include some very deep but short PMDs
- `sample_name`: A label currently associated with the methylome. These are
  inferred based on annotations in SRA and GEO, and depend heavily on how the
  original data submitter chose to describe their data. There have been cases
  where these are wrong because, for example, all samples in the same study
  were given a name related to the study/project, rather than the individual
  sample, and only detailed reading can uncover the differences. Your feedback
  can help improve these.

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

### Initialize logger

Initialize logging for transferase. This function is called automatically when
`library(Rxfr)` is loaded. You will never need to use it, and it takes
no arguments anyway.

Usage:
```R
init_logger()
```

Examples:
```R
init_logger()
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
`set_xfr_log_level`, or whatever log level you most recently set it to.  The
return value is a string indicating the log level. Example:
```R
get_xfr_log_level()
```

## MClient

The MClient class is one of two classes in the R API. MClient is an R6 class
that serves as the interface for making remote transferase queries. You will
not be able to instantiate an object of MClient class until you have either
run the `config_xfr` function, or configured transferase using the command
line app (or did the same with the Python API).

### Data:

* `config_dir`: The name of the configuration directory associated with this
  MClient object. Again, this would have been configured using `config_xfr` or
  the command line transferase app.

* `client`: A pointer to the underlying object, which cannot be inspected
  directly. This object could be several hundred MB in size, depending on how
  many genomes you are working with.

### Constructor:

The constructor can take the name of a configuration directory, but most users
will leave this empty and allow it to use the default.

Arguments:
* `config_dir`: The name of a configuration directory; most users will
  typically leave this empty.

Examples:
```R
config_xfr(c("hg38", "mm39"), "some_directory")
client <- MClient$new("some_directory")
```

### do_query

Query transferase

This is the central function for querying methylomes using transferase.  It
takes methylome names and query intervals. It returns methylation levels for
each query interval and methylome, in a matrix with rows corresponding to
query intervals and columns corresponding to methylomes. It allows for query
intervals to be specified in multiple formats and has options that control how
the output is structured.

Currently only "remote" queries are supported through the R package, but that
does include a server you run on your own machine (e.g., your laptop, in a
different window). This is explained in the usage docs for the transferase
command line app.

Usage:
```R
do_query(
    methylomes,
    query,
    genome = NULL,
    covered = FALSE,
    add_n_cpgs = FALSE,
    add_header = FALSE,
    add_rownames = FALSE,
    header_sep = '_',
    rowname_sep = '_'
)
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
* `add_n_cpgs`: If TRUE, the total number of CpG sites in each query interval
  with be provided as the final column in resulting matrix.
* `add_header`: If true, a header will be added to the resulting matrix.
* `add_rownames`: If true, rownames will be added to the resulting matrix.
* `header_sep`: The separator character to use inside column names in the
  resulting matrix.
* `rowname_sep`: The separator character to use inside row names in the
  resulting matrix.

Return value: A numeric matrix with one row for each query intervals, and
columns corresponding to the methylomes in the query. For each query
methylome, there will be columns named with suffixes `_M` and `_U` indicating
the number of methylated and unmethylated observations, respectively.
Additionally, if `covered = TRUE` was set, then there will be a third column,
indicated with the suffix `_C`, to indicate the number of covered CpG sites in
each intervals. The underscore in `_M`, etc., can be changed using the
`header_sep` argument.

Details:

The do_query method for MClient objects needs to know the following:

- The names of methylomes you want to query.
- An indication of the query intervals you want information about.

The query intervals can be specified in 3 ways:

- A data frame with each query interval specified in the first 3 columns,
  giving the chromosome, the start position and the end position,
  respectively. The chromosome must exist for that genome, and the start and
  stop positions are considered 0-based and half-open intervals.
- An integer bin size. I use the word "bin" here as opposed to "window"
  because the bins don't slide: they are non-overlapping. There is a minimum
  bin size to ensure that no user asks for bins of 1bp, which would result in
  a lot of empty information sent from the server.
- A formatted MQuery object (see the documentation for the MQuery class). This
  is an object made from a set of genomic intervals, but allows for faster
  queries. This is because the MQuery object is created for every query that
  is based on a data frame of intervals. If the MQuery object is constructed
  separately, it can be used repeatedly without recreating it.

If the query intervals are not specified as an MQuery object, the
corresponding reference genome must also be supplied.

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

Create an MQuery object from genomic intervals

This method formats a set of query intervals as a MQuery object.
The query intervals must be a data frame specifying chromosome, start and stop
for each interval in the first three columns, respectively. Intervals are
0-based and half-open. Intervals must be sorted within each chromosome, but
the order of chromosomes does not matter.

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

There are no meaningful methods associated with MQuery object that you should
be using. The only uses of MQuery objects is passing them to the `do_query`
method of the MClient class.

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
