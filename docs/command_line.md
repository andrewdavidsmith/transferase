# Transferase command line tools

The transferase command line app has an alias `xfr`, which is installed
alongside the `transferase` binary. I use `xfr` because it's quicker to
type. There are several commands available through `xfr`, and all have to do
with finding methylation levels through genomic intervals. The best way to
understand what this means is to understand "methylation levels" by reading
this brief article:

```
"Leveling" the playing field for analyses of single-base resolution
DNA methylomes Schultz, Schmitz & Ecker (TIG 2012)
```

And in the context of genomic intervals, the methylation level through a
genomic interval is the same as outlined in the docs for `dnmtools roi`
[here](https://dnmtools.readthedocs.io/en/latest/roi).

Quickstart
----------

This is a quickstart to access to MethBase2 methylomes remotely through the
transferase server. You should be able to start getting methylome data in
under a minute. If, like me, you sometimes copy and paste commands without
reading what they do, feel free here. In this section it will cost you 200MB
and about 30s. But if that's you, then you aren't reading this anyway. Only
one of the `xfr` commands will blow up your computer, but that feature is not
documented.

### Configuring transferase

Before starting, you should do a quick configuration. It is possible to use
most of the commands in `xfr` without doing this configuration.  That will be
explained below. But doing the configuration is pretty quick and makes
everything simpler simpler. Here is the command:

```console
xfr config -g hg38,mm39
```

This will give you a default configuration, and put it in the default place
(`{$HOME}/.config/transferase`). It will download (via regular web download)
some information for the genomes hg38 and mm39, which are the assemblies used
for human and mouse in MethBase2. The amount of data downloaded for this step
is about 200MB and might take about 30 seconds, depending on your internet
speed. If you are interested in other species, check out which ones are
available through MethBase2.

### Selecting methylomes

Quickest way to start: skip this section and just use the methylomes names
already in the commands in the following sections.

Before doing a query you need 2 things: (1) the name of at least one
methylome, and (2) a file with genomic intervals.

The methylomes in MethBase2 are all "named" according to a SRA accession
number. The study "SRP08344", from Hodges et al. (2011) included methylomes
for 4 types of blood cells, with SRX096522 and SRX096523 being two technical
replicates from pooled blood of multiple people. These are both in MethBase2
as individual methylomes, but the names SRX096522 and SRX096523 are not easy
to remember. If you know the paper, you can look up the accessions for
"experiments" and they are likely available in MethBase2. You could also look
at the methylomes from MethBase2 in the list at the UCSC Genome Browser.

If you don't like the "labels" you see in this app, I agree, but please read
the final section under "Labels".

Transferase comes with tool that can help you get started. The following
command will bring up an interface to allow you to select methylomes based on
the associated biological sample, in this case it would be fore human:

```console
xfr select -g hg38 -o selected_methylomes.txt
```

If this command doesn't work for you at all, please see the footnote at the
bottom of this document titled "Terminals" for some tips.

You will be asked to confirm before proceeding, and then you will see a list
of methylomes. For human (hg38) there will be a long list of roughly 5,500
methylomes (as of Feb 1, 2025), each with some information about the
biological sample. You can navigate the list with the arrow keys,
page-up/down, home and end. You can add a methylome with the space bar, or
add/remove multiple at once by switching modes (there is a legend at the top
of the screen and help by typing 'h'). Importantly, you can search the list,
and then select methylomes matching your query. The search is not
case-sensitive, but "b cell" and "b-cell" are currently considered
different. When you quit, your selection will be saved in
`selected_methylomes.txt`, or whatever output file you specified.

You can select several methylomes this way and use them in the commands
below. Don't try to select all samples -- that will be too much data and the
server will currently reject queries using more than roughly 45 methylomes at
once.

### Doing a query

You will need some intervals. If you do any genomics, you probably have
several different files that list genomic regions of interest to you -- and in
BED format, maybe 3 or 6 column (any with at least 3 columns will work; the
first 3 columns must be chrom, start and stop, half-open and zero-based). If
you don't, you can download
[this](https://smithlabresearch.org/transferase/other/cpgIslandExtUnmasked_hg38.bed3)
file name `cpgIslandExtUnmasked_hg38.bed3`.

```console
xfr query -g hg38 -m SRX096522 -i cpgIslandExtUnmasked_hg38.bed3 -o results.bed
```

The file `results.bed` will have the counts of methylated and unmethylated
reads for each interval. It looks like this:

```
$ head results.bed
chr1    10468   11240   20      23
chr1    28735   29810   0       38
chr1    51587   51860   44      6
chr1    135124  135563  78      8
chr1    180998  181759  341     100
chr1    199251  200253  0       0
chr1    368746  370063  12      0
chr1    381172  382185  34      1
chr1    491107  491546  0       0
chr1    597781  598734  77      11
```

Above, the 4th column is the number of methylated reads and the 5th is the
number of unmethylated reads.

You have several options for the output. Each can be specified as either a
number or word:

* counts (1): The default. What you see above.
* bedgraph (2): The first number divided by the sum of both -- known as
  weighted mean methylation.
* dataframe (3): This will make the first column work like rownames in an R
  data frame, and it will also give column headings.
* dfscores (4): Same as "dataframe" but with the scores that are like
  "bedgraph". There is a parameter that allows for "NA" values to be used when
  the counts would have been too low for an interpretable fraction.

Just as the total counts being too low means that fractions are tough to
interpret (a standard error issue), if the number of CpG sites in the interval
are too low, you might also want to take that into account. There is an option
`--covered` for `xfr query` that will give you an additional column indicating
the number of CpG sites in each interval that have at least one read. The
output would look like this.

```console
xfr query -g hg38 -m SRX096522 \
  -i cpgIslandExtUnmasked_hg38.bed3 -o results_covered.bed --covered
```

```
$ head results_covered.bed
chr1    10468   11240   20      23      16
chr1    28735   29810   0       38      28
chr1    51587   51860   44      6       29
chr1    135124  135563  78      8       16
chr1    180998  181759  341     100     96
chr1    199251  200253  0       0       0
chr1    368746  370063  12      0       12
chr1    381172  382185  34      1       35
chr1    491107  491546  0       0       0
chr1    597781  598734  77      11      23
```

The `--covered` option is only relevant for `counts` and `dataframe` format
output.

There is another kind of query that doesn't require a file with genomic
intervals. These are called "bins" queries. The bins are non-overlapping
genomic intervals of equal size that cover the genome.  These are not sliding
windows. To do a bins query, simply replace the intervals file your query with
a bin size. Here is an example, using some of the other options:

```console
xfr query -g hg38 -b 10000 -o results_df.txt --out-fmt dfscores --covered \
    --min-reads 10 -m SRX096522 SRX096523
```

The output should look like this:
```
$ head results_df.txt
SRX096522       SRX096523
chr1.0.10000    NA      NA
chr1.10000.20000        0.797235        0.781282
chr1.20000.30000        0       0.023622
chr1.30000.40000        NA      NA
chr1.40000.50000        0.9     0.897959
chr1.50000.60000        0.822917        0.784615
chr1.60000.70000        0.872727        0.896226
chr1.70000.80000        0.769231        0.75
chr1.80000.90000        0.793103        0.839695
```

Note: as suggested by the words `dataframe` and `dfscores`, this format is an
R data frame, so it can be loaded with `read.table("results_df.txt")` in R.

Be aware that if you try to use a bin size that is too small, the server will
reject your query. At the same time, if your bin size is too small most of the
bins will have no CpG sites. If the server were to allow bins queries with bin
size of 100bp, then the output for just one methylome would approach 1GB.

Working with your own data
--------------------------

I think this is really worthwhile if your DNA methylation project needs
exploratory data analysis. There are several steps to this. None take much
computing time, but I would guess this might take half an hour to setup due to
this poor documentation. I'll give a better estimate once I have some
feedback. Unfortunately the starting point involves other tools,
[dnmtools](https://github.com/smithlabcode/dnmtools). I'm happy to answer any
questions about dnmtools, but their use won't be covered here.

### Making a genome index

Everything requires a genome index. If you are working with your own data,
make a genome index from the exact reference genome you used to map your
reads. It's a bad idea to assume that the genome index downloaded with `xfr
config` will correspond to the exact same reference genome you used. A
difference of even a few bp, or the inclusion of some different sequence
(unassembled, alternate haplotype, etc.), could make the results very
wrong. The computing for this step should be quick -- 10 seconds for a human
genome on my laptop.

To make the index, you need the reference genome in a single fasta format
file. If your reference file name is `hg38.fa`, in your current directory,
then do this:

```console
xfr index --genome hg38.fa --index-dir my_indexes
```

This command will create two files, and they should be kept together.  In
subsequent `xfr` commands, you will give the genome name and the directory, so
you won't be specifying exact names of these files. But I'll explain just a
bit. The first one is the genome index data, which would be named
`my_indexes/hg38.cpg_idx` if you used the above command. It is binary, and if
`hg38.fa` is roughly 3.0GB, then this file will is about 113MB in size. The
other file is the genome index metadata, in JSON format. It would be named
`my_indexes/hg38.cpg_idx.json`. You can view it with any JSON viewer.  There
is no guarantee it will be in pretty-print format, but doing something like
`cat my_indexes/hg38.cpg_idx.json | jq` would make it readable. There is no
reason to read it.

After you made an index file (or multiple), you can see the available genomes
for use with transferase like this:

```console
xfr list my_indexes
```

It will show you the names of genomes with indexes in the specified
directory. More on this later.

*Note 1* the `index` command will also work with gzip format reference genome
files, like `hg38.fa.gz`.

*Note 2* if you are working through this document as an example, and not with
your own data, you might want to use the above command on the file available
here, as it is the one needed if you use the other example data files in later
steps (a 4-5min download with my home internet speed):

* [hg38.fa.gz](https://smithlabresearch.org/transferase/other/hg38.fa.gz) (826M)

### Making a methylome file

The starting point to make a methylome file with single-nucleotide methylation
levels. These must be in terms of counts, for the CpG sites. These are assumed
to be for the "symmetric" CpG sites, so they must be indexed from 0, and
reference only the `C` of the CpG.  Several commands in `dnmtools` generate
files like this from mapped reads (e.g., BAM format). If you are uncertain
about this, please see the documentation for the `counts` dnmtools command
[here](https://dnmtools.readthedocs.io/en/latest/counts).

If you want one for testing, you can download these random examples (and
please let me know if you arrive at a dead link):

* [SRX9531739.xsym.gz](https://smithlabresearch.org/transferase/other/SRX9531739.xsym.gz) (70M)
* [SRX9531751.sym.gz](https://smithlabresearch.org/transferase/other/SRX9531751.sym.gz) (142M)

Both are gzip compressed. The file with extension `.xsym.gz` is in "xcounts"
format and the other is in "counts" format. Both encode methylation levels at
each symmetric CpG site in the human genome for the given methylomes. Both of
these formats are produced by dnmtools, and clearly one is smaller than the
other.

If you are using the example files, then be sure you made a genome index for
the `hg38.fa.gz` reference genome downloaded from the link in the previous
step. This is essential, because it defined the correspondence between the
files `SRX9531739.xsym.gz` and `SRX9531751.sym.gz` and the genome index. Here
is the command to get a transferase format methylome:

```console
xfr format -x my_indexes -g hg38 -m SRX9531739.xsym.gz -d my_methylomes
```

Be careful about the `-x` argument, because if you forget it, then `xfr` might
find a genome index with the same name in your transferase config directory,
which would be setup if you did the "quickstart" at the top of this file.

Similar to the genome index, a methylome in transferase format involves two
files. They will both be placed in the output directory (`my_methylomes`) and
whenever they are used they will be used together. Again, as with the genome
index, one file is binary and the other is JSON. You can read that JSON file
if you want, but there is no reason to read or modify it.

After you made a transferase format methylome with the output directory
`my_methylomes` you can use the `list` command to see the methylomes available
in that directory:

```console
xfr list my_methylomes
```

It will show you the names of methylomes in the specified directory.
Remember: any time a methylome is used in transferase, a genome index must be
available for the reference genome used to create that methylome. If you are
doing remote queries, the servers can take care of this for you. But for local
use with your own data, you need to make sure of this.

### Running the query command locally

As with the remote query, you need some intervals. You can reuse the same
ones. If you named the directories `my_indexes` and `my_methylomes` in earlier
steps, and used all the example data files, then this is the command:

```console
xfr query --local \
    -x my_indexes -g hg38 -d my_methylomes -m SRX9531739 \
    -o output.bed -i cpgIslandExtUnmasked_hg38.bed3
```

The genome index for hg38 and the methylome for SRX9531739 are the same as
explained above (each is two files). The output in the `output.bed` file
should be consistent with the information in the command:

```console
dnmtools roi -o output.roi cpgIslandExtUnmasked_hg38.bed3 SRX9531739.xsym.gz
```

The format of these output files are different, but the methylation levels on
each line (i.e., for each interval) should be identical.  Note that `dnmtools
roi` can fail if intervals are nested, while `xfr query` command will still
work.

### Checking data consistency

If you are analyzing your own data, there is a command that can help ensure
your data makes sense. Assuming you have the same `my_indexes` and
`my_methylome` directories as in the previous examples, then the command:

```console
xfr check -x my_indexes -d my_methylomes
```

Will check for internal consistency of each genome index in `my_indexes`, each
methylome in `my_methylomes`, and will check that the genome index
corresponding to each methylome is present, and that each methylome is
consistent with the corresponding genome index.

Running a transferase server
----------------------------

The `server` command can be run locally, on your laptop.  The easiest way is
just to open two terminal windows: one for the server and the other to do your
data analysis.

The steps here assume you have the genome index for hg38 and a methylome named
SRX9531751. If you have some other genome or methylome, it's fine, just
substitute the names.

### Querying your local server

For this example, your methylomes should be in a directory named
`my_methylomes` and your genome indexes should be in `my_indexes`. The
`my_indexes` directory should contain genome indexes for each genome
associated with a methylome in `my_methylomes`. However, if you never make a
query related to a particular methylome, then the server won't care that its
corresponding genome index is missing.

For now, using names and data from the above examples, we would have a single
index and two methylome. Here is a command that will start the server:

```console
xfr server -v debug -s localhost -p 5000 -d my_methylomes -x my_indexes
```

Above, the arguments indicate to use the host "localhost" with port 5000. Note
that this will fail with an appropriate error message if port 5000 is already
be in use, and you can just try 5001, 5002, etc., until one is free. The
hostname can also be specified numerically (in this example, 127.0.0.1). The
`-v debug` will ensure you see info beyond just the errors. This information
is logged to the terminal by default, but you can set a log file. The server
can also be run in detached mode, but if you don't know what that means, you
don't likely have any reason to do it.

### Querying your local server

This is really great if you are doing exploratory data analysis, wrapping the
transferase commands into a pipeline that makes multiple queries. The reason
is that the transferase server can keep each methylome in memory, and after
the first query you make, subsequent queries will not need to load the
data. If you have an 8GB laptop, you can easily keep 50 methylomes live at the
same time, and many queries will be almost instantaneous.

Assuming you started the server in a different terminal window as explained
above, you will need to generate a configuration to query it. This command
will do the configuration and also download the genome index for hg38:

```console
xfr config -c my_config -s localhost -p 5000 -g hg38
```

With this configuration, you can do a query almost exactly as if you were
querying MethBase2 on the transferase server:

```console
xfr query -c my_config -g hg38 \
    -m SRX9531739 -o output.bed -i cpgIslandExtUnmasked_hg38.bed3
```

In this case, from the perspective of your query, "SRX9531739" is not a file,
just the name of a methylome. If you want to confirm that everything works,
compare the output from running the query command remotely vs. locally and
check that the output is the same for the example methylomes provided via
download links above.

If you made it this far with your own server, check out the server
documentation (as of now, in a file named `server.md`), which includes
information about how to optimize performance.

# Other

**Labels** The "labels" we associate with each methylome in MethBase2, which
you can browse using the `xfr select` command, are only as good as the
information people upload to GEO and SRA at NCBI (or ENA or DDBJ). Just prior
to writing this I found methylomes labeled with the taxonomic ids for
orangutan and macaque, while the sample descriptions that the samples were
taken from chimp. Just be aware. Several times I've seen identical text pasted
in for different biological samples which simply covers all samples in the
same study, sometimes not including any word that is related to the particular
sample. This is also not the fault of data submitters -- fields like "sample
name", "isolate", "biomaterial provider", "cell type", "tissue", "cell line",
"source name" and more are all available to use, and in some cases it can be
difficult to decide which one to use.

**Terminals** I ran into various problems when using the `xfr select` command
across machines. All of this assumes that `xfr` works in general, but that the
`xfr` command tells you something like this when you try to run it:

```
Error opening terminal: screen-256color
```

Here are some things to try:

1. Try to sepcify a terminal that `xfr` understands without making any general
   changes to your session:
   ```
   TERM=xterm xfr select -g hg38 -o out.txt
   ```
   On one of my systems I need to do this when I'm using tmux. I also figured
   out how to eliminate this problem for versions after 0.5.0.

2. The terminal configuration file is not being found. I had this issue due to
   conda assigning the environment variable TERMCAP, which helps terminals
   find their configuration files. I can't reproduce it now, but the issue was
   solved by reassign the TERMCAP variable to the original system location:
   ```
   TERMCAP=/usr/share/terminfo xfr select -g hg38 -o out.txt
   ```

The use of `xfr select` as a terminal app was just a quick way to get an app
running that didn't require adding code for a local browser app or setting up
a web system for the selection. But I think one of those will eventually be
needed. I'm very happy to accept advice on making this more robust.
