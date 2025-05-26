[**Quickstart**](#quickstart) |
[**Queries**](#queries) |
[**Selecting methylomes**](#selecting-methylomes) |
[**Your data**](#your-data) |
[**Your server**](#your-server)

# The transferase command line app

Transferase is about methylation levels in genomic intervals. The best way to
understand what this means is to understand "methylation levels" by reading
[this](https://pmc.ncbi.nlm.nih.gov/articles/PMC3523709) brief article:

```text
"Leveling" the playing field for analyses of single-base resolution
DNA methylomes Schultz, Schmitz & Ecker (TIG 2012)
```

In the context of genomic intervals, the methylation level is the same as
explained for `dnmtools roi`
[here](https://dnmtools.readthedocs.io/en/latest/roi).

The transferase command line app is named xfr because it's quicker to type.

## Quickstart

You should be able to start getting methylome data in under a minute. If, like
me, you sometimes copy and paste commands without reading what they do, feel
free. But if that's you, then you aren't reading this anyway. Only one of the
`xfr` commands will blow up your computer, but that feature is not documented.

1. Configure transferase: For human, do this:

   ```console
   xfr config -g hg38
   ```

   - By default configuration files are put in `$HOME/.config/transferase`.
   - Configuring the human genome takes 130MB and around 30s, depending on internet
     speed.
   - You can configure multiple genomes, for example `-g hg38,mm39`. See available
     genomes [here](https://github.com/smithlabcode/methbase#genomes).

2. Select methylomes: To start quickest, skip this and use whatever appears in
   the commands below.

   Or you can select methylomes based on biological sample with this command:

   ```console
   xfr select -g hg38 -o selections.txt
   ```

   Use 'd' to show sample details, select methylomes with the space bar, save
   your selections with 'w', and get help with 'h'. More on 'select'
   [here](#selecting-methylomes).

3. Query intervals: You need a BED file. If you don't have one close by,
   get human CpG islands
   [here](https://smithlabresearch.org/transferase/other/cpgIslandExtUnmasked_hg38.bed3):

   ```console
   wget https://smithlabresearch.org/transferase/other/cpgIslandExtUnmasked_hg38.bed3
   ```

4. Go!

   ```console
   xfr query -g hg38 -i cpgIslandExtUnmasked_hg38.bed3 -o output.txt \
       -m SRX096520 SRX096521 --scores
   ```

   If you used 'select' to get a `selections.txt` file, put it in place of
   `SRX096520 SRX096521`. If you have your own BED file, put it in place of
   `cpgIslandExtUnmasked_hg38.bed3`.

5. If you copy-pasted everything so far, the first 10 lines of your
   `output.txt` should look like:

   ```console
   $ head output.txt
   SRX096520   SRX096521
   chr1.10468.11240    0.862275    0.780303
   chr1.28735.29810    0.0229885   0.0389105
   chr1.51587.51860    0.910256    0.767442
   chr1.135124.135563  0.942029    0.898551
   chr1.180998.181759  0.826305    0.790168
   chr1.199251.200253  0.0588235   0
   chr1.368746.370063  0.94    0.96875
   chr1.381172.382185  0.896   0.867133
   chr1.491107.491546  0   0
   ```

   Each value is the methylation level for a genomic interval in a methylome.

Summary:

- We used the config, select and query commands. Those might be all you need,
  but there are more.

- We used the public transferase server. The config command sets this up by
  default. You choose the genomes to configure.

- The public transferase server gives you access to methylomes from
  [MethBase2](https://github.com/smithlabcode/methbase#genomes), which is big
  and growing.

- The methylome names are SRA accessions and connect to biological or clinical
  metadata. Those names are not so memorable. The select command can fix
  that. And make other things easier.

- There are [many options](#output) to adjust the information you get from a
  query and how the output is formatted.

- If you have your own sequencing-based methylomes, you can run your own
  transferase server, for your lab, or just for yourself on your laptop.

- Everything covered in this quickstart can be done using the transferase
  Python and R packages.

## Queries

This is the central command in transferase. This section explains the options
and gives examples. I'll start with a very basic query, and then elaborate.

```console
xfr query -g hg38 -i cpgIslandExtUnmasked_hg38.bed3 -o output.txt -m SRX096520 SRX096521
```

This query is minimal in that it specifies the genome, query intervals, a
methylome, and an output file.

### Output

These options relate to the output:

```text
  -o,--output FILE            output filename (directory must exist)
  --scores                    scores output format
  --classic                   classic output format
  --counts                    counts output format (default)
  --covered                   count covered sites for each reported level
  --bed                       no header and first three output columns are BED
  --dataframe                 output has row and column names
  --cpgs                      report the number of CpGs in each query interval
  -r,--reads INT              minimum reads below which a score is set to NA
```

The first option specifies the name of the output file and is required. If the
output filename includes a directory part, that directory must exist.

The output format is a table of methylation levels, with one row corresponding
to each query interval and columns corresponding to methylomes. Here's the output
of the query at the start of this section:

```console
$ head output.txt
SRX096520_M     SRX096520_U     SRX096521_M     SRX096521_U
chr1.10468.11240        144     23      206     58
chr1.28735.29810        2       85      10      247
chr1.51587.51860        142     14      99      30
chr1.135124.135563      325     20      62      7
chr1.180998.181759      823     173     659     175
chr1.199251.200253      1       16      0       11
chr1.368746.370063      94      6       31      1
chr1.381172.382185      112     13      124     19
chr1.491107.491546      0       0       0       0
```

These values are different from those in the quickstart, but these integer
counts are the full information about the methylation levels. This format is
concise, but you can customize it. If you use R, this overall structure will
be recognizable as a data frame.

The output format will be either:

**Data frame** These are data frames intended for convenient use in R. They
have column names (a header) for all but the first column, and the row names
as the first column. You can read these in R with `read.table("output.txt")`
and in Pandas with `pd.read_csv("output.txt", sep="\t")`. This is the default,
and can be made explicit by specifying `--dataframe`.

**BED-like** These have no column headers. The columns are ordered according
to the order given for query methylomes (or the sorted order on "labels" if
those are supplied), and then follow the rules below. Each row includes the
3-column BED interval as the first 3 columns of the output. You need to
specify `--bed` to get this format. An example using `--bed` will be given
below.

The output format will also be one of:

**Classic** For each methylome involved in a query, there are at least two
output columns. (1) The score: a fraction equal to `n_meth/(n_meth +
n_unmeth)`, where `n_meth` is the number of methylated observations in
sequenced reads, and `n_unmeth` is similarly the number of unmethylated
observations. (2) The read count, which is `n_meth + n_unmeth`. If the output
is a data frame, then the score column name will have the suffix `_M` and the
read counts column name have the suffix `_R`. If the query requests the number
of covered sites per interval per methylome, then this information will be in
a third column, having the suffix `_C`. The advantage of this format is that
the fractions are immediately available. The disadvantage is that these files
can be larger, and take longer to read and write. Get this by specifying
`--classic`.

Here is an example of the classic output format:

```console
xfr query -g hg38 -i cpgIslandExtUnmasked_hg38.bed3 -o output.txt \
    -m SRX096520 SRX096521 --classic
```

And the output would look like this:

```console
$ head output.txt
SRX096520_M     SRX096520_R     SRX096521_M     SRX096521_R
chr1.10468.11240        0.862275        167     0.780303        264
chr1.28735.29810        0.0229885       87      0.0389105       257
chr1.51587.51860        0.910256        156     0.767442        129
chr1.135124.135563      0.942029        345     0.898551        69
chr1.180998.181759      0.826305        996     0.790168        834
chr1.199251.200253      0.0588235       17      0       11
chr1.368746.370063      0.94    100     0.96875 32
chr1.381172.382185      0.896   125     0.867133        143
chr1.491107.491546      0       0       0       0
```

Notice that the column headings now have `_M` and `_R`, meaning methylation
level (not count of methylated reads), and total number of reads. This is the
same information as in the `--counts` (default) format, but it might be more
convenient to start with this format.

**Counts** For each methylome involved in a query, there are at least two
output columns.  (1) The count `n_meth` of methylated reads, and (2) the count
`n_unmeth` of unmethylated reads. Clearly these two counts can be used to
obtain the same information as in the 'classic' format. The advantage of this
format is that it's smaller, so disk read and writes are faster. The
disadvantage is that if you want the "score" you need to do the division
yourself. If the output is in data frame format, then the `n_meth` column has
a `_M` suffix, and the `n_unmeth` column has a `_U` suffix. If you request
sites covered (using `--covered`), then a third column will be present, as
explained for 'classic' above. This is the default, and can be made explicit
with the `--counts` argument.

Here's an example of using the counts output format with the `--covered`
option to get the number of CpG sites covered by at least one read in each
query interval:

```console
$ xfr query -g hg38 -i cpgIslandExtUnmasked_hg38.bed3 -o output.txt \
      -m SRX096520 SRX096521 --covered
$ head output.txt
SRX096520_M     SRX096520_U     SRX096520_C     SRX096521_M     SRX096521_U     SRX096521_C
chr1.10468.11240        144     23      58      206     58      89
chr1.28735.29810        2       85      27      10      247     43
chr1.51587.51860        142     14      29      99      30      29
chr1.135124.135563      325     20      20      62      7       20
chr1.180998.181759      823     173     107     659     175     107
chr1.199251.200253      1       16      6       0       11      8
chr1.368746.370063      94      6       45      31      1       15
chr1.381172.382185      112     13      64      124     19      63
chr1.491107.491546      0       0       0       0       0       0
```

Notice that both methylomes have an additional column, suffixed with `_C`,
which incidates the number of CpG sites in each query interval that have at
least one read covering them. Looking at the final entry `chr1.491107.491546`,
we might suspect there are no CpG sites in that intervals. Except in these
examples we are working with CpG islands. We can use the `--cpgs` option to
report the number of CpGs in each interval as the final column:

```console
$ xfr query -g hg38 -i cpgIslandExtUnmasked_hg38.bed3 -o output.txt \
      -m SRX096520 SRX096521 --covered --cpgs
$ head output.txt
SRX096520_M     SRX096520_U     SRX096520_C     SRX096521_M     SRX096521_U     SRX096521_C     N_CPG
chr1.10468.11240        144     23      58      206     58      89      115
chr1.28735.29810        2       85      27      10      247     43      116
chr1.51587.51860        142     14      29      99      30      29      29
chr1.135124.135563      325     20      20      62      7       20      30
chr1.180998.181759      823     173     107     659     175     107     107
chr1.199251.200253      1       16      6       0       11      8       112
chr1.368746.370063      94      6       45      31      1       15      102
chr1.381172.382185      112     13      64      124     19      63      84
chr1.491107.491546      0       0       0       0       0       0       29
```

The number of CpGs in an interval is not a property of the methylome, but of
the reference genome, so this column only appears once. It will always be the
final column.

**Scores** This format is similar to 'classic' except it does not include the
read count column. This makes the format slightly smaller, and more convenient
to work with (e.g., you can do PCA in R with very few steps).  However, any
query interval for which there are no reads will have an undefined value, and
without the read count column, you wouldn't be able to distinguish this
situation from a very confident low methylation level. The `--min-reads`
parameter will ensure that a value of `NA` is used when the number of reads
fails to meet your cutoff for confidence. If you only have one methylome in
your query, use `--bed` and let `--min-reads` be zero, then your query output
will be a bedgraph. Get this by specifying `--scores` and keep `--min-reads`
in mind.

Here's the example from the quickstart using `--scores` but not specifying
`--min-reads`:

```console
$ xfr query -g hg38 -i cpgIslandExtUnmasked_hg38.bed3 -o output.txt \
      -m SRX096520 SRX096521 --scores
$ head output.txt
SRX096520       SRX096521
chr1.10468.11240        0.862275        0.780303
chr1.28735.29810        0.0229885       0.0389105
chr1.51587.51860        0.910256        0.767442
chr1.135124.135563      0.942029        0.898551
chr1.180998.181759      0.826305        0.790168
chr1.199251.200253      0.0588235       0
chr1.368746.370063      0.94    0.96875
chr1.381172.382185      0.896   0.867133
chr1.491107.491546      0       0
```

Notice first that the column headers have no suffix. That's because there is
only one column for each query methylome. Notice also that the final interval
has methylation levels of 0. From the prevous examples we know this must be
true because in the two query methylomes, there is no data for that
interval. If we specify a value for `--min-reads` we can be more confident in
those fractions:

```console
$ xfr query -g hg38 -i cpgIslandExtUnmasked_hg38.bed3 -o output.txt \
      -m SRX096520 SRX096521 --scores -r 10
$ head output.txt
SRX096520       SRX096521
chr1.10468.11240        0.862275        0.780303
chr1.28735.29810        0.0229885       0.0389105
chr1.51587.51860        0.910256        0.767442
chr1.135124.135563      0.942029        0.898551
chr1.180998.181759      0.826305        0.790168
chr1.199251.200253      0.0588235       0
chr1.368746.370063      0.94    0.96875
chr1.381172.382185      0.896   0.867133
chr1.491107.491546      NA      NA
```

By specifying `--min-reads 10` (or with `-r`) we essentially require 10 "coin
flips" to estimate the methylation level, which is like the weight on a coin.
The `NA` is for consistency with `R`, and Pandas also understands this. Unlike
`--counts` and `--classic`, using `--scores` discards information, but it
still might be the most convenient format for the next step in your analysis.

More on the `--covered` option: Just as the total counts being too low means
that fractions are tough to interpret, if the number of CpG sites in the
interval is too low, you might also want to take that into account. The
`--covered` option has no effect if `--scores` is specified. It is only
relevant for `--counts` and `--classic`.

### Intervals

All queries need a set of genomic intervals. You can specify query intervals
in two mutually exclusive ways:

```text
  -i,--intervals FILE         input query intervals file in BED format
  -b,--bin-size INT           size of genomic bins to query
```

The genomic intervals are usually given in a BED format file. If you do
genomics, you probably have many different BED files that list genomic
intervals of interest to you -- for example the promoters of genes in your
favorite pathway. There are variations on BED format, but transferase only
requires that the first 3 columns are valid as 3-column BED format, which is
documented [here](https://genome.ucsc.edu/FAQ/FAQformat.html#format1).

If you try to use intervals that run past the ends of chromosomes, possibly
because you mixed up two species, you will get an error. Transferase only
deals with "primary" chromosomes, which are usually the autosomes, the sex
chromosomes and mitochondria (and chloroplast, when the plant methylomes in
MethBase2 are made public). Any chromosome names with "chrUn" or "random"
result in errors if they are used with the public transferase server. If you
run your own server, they are fine as long as they were in your original
reference genome.

Intervals are assumed to be **zero-based and half-open**, which of course is
the only sensible way. In transferase, CpG sites are symmetric and the
methylation is associated with the reference strand. So if you use query
intervals that are 1-based you might miss the first CpG you intended to
include. Here is the download
[link](https://smithlabresearch.org/transferase/other/cpgIslandExtUnmasked_hg38.bed3)
again for the example from the quickstart:

```console
wget https://smithlabresearch.org/transferase/other/cpgIslandExtUnmasked_hg38.bed3
```

From the filename, these are obviously CpG islands from hg38 and CpG islands
begin and end at CpG sites by definition. You can verify this by uploading
this file as a custom track in the UCSC Genome Browser, zoom in on the ends of
any interval, and notice that they begin at the C of a CpG, and end covering
the G of a CpG. For transferase, all that would be required is that the end
cover the C of the final CpG.

The alternative to supplying a BED format intervals file is giving a bin size
with the `--bin-size` option. Bins have fixed size, cover the whole genome,
and do not overlap. The term "bin" is in contrast to "window": windows slide
but we use bins when making histograms.

Bins are useful if you want to do global comparisons between methylomes, for
example on a scale 100kbp. Although the server can do the calculations for
bins very quickly, the amount of data it needs to send back can be very
large. If you attempt a query with 1000bp bins, and 10 methylomes, you are
asking for roughly 30,000,000 values to be sent back to you from the
server. Although transferase has built-in compression, a "bins" query with too
small a bin size is asking for a lot of junk -- consider how much of a
mammalian genome has very few CpG sites. This will lead to large files that
you will struggle to use in downstream analysis. There are several ways to
improve this situation, but the goal for transferase is not to more
efficiently tell users where CpG are absent.

Here is a query that uses the `--bed` option, includes only one methylome, and
specifies query intervals with a bin size:

```console
xfr query -g hg38 -b 100000 -o output.txt -m SRX096520 --scores --bed
```

The output looks like this:

```console
# head output.txt
chr1    0       100000  0.80415
chr1    100000  200000  0.837388
chr1    200000  300000  0.798559
chr1    300000  400000  0.820157
chr1    400000  500000  0.795252
chr1    500000  600000  0.788016
chr1    600000  700000  0.176634
chr1    700000  800000  0.629851
chr1    800000  900000  0.699224
chr1    900000  1000000 0.55425
```

This is [bedgraph](https://genome.ucsc.edu/goldenpath/help/bedgraph.html)
format. Currently the `--dataframe` format output for bins has redundancy
in the rownames:

```console
$ xfr query -g hg38 -b 100000 -o output.txt -m SRX096520 SRX096521 --scores -r 10
$ head output.txt
SRX096520       SRX096521
chr1.0.100000   0.80415 0.700608
chr1.100000.200000      0.837388        0.793026
chr1.200000.300000      0.798559        0.74281
chr1.300000.400000      0.820157        0.81174
chr1.400000.500000      0.795252        0.712474
chr1.500000.600000      0.788016        0.81742
chr1.600000.700000      0.176634        0.222358
chr1.700000.800000      0.629851        0.614279
chr1.800000.900000      0.699224        0.67415
```

This redundancy is planned for removal in future versions.  **Update**
(05/25/2025) This has been fixed in the main branch so that the second
(redundant) bin end is no longer shown when output is dataframe.


### Methylomes

Each query must specify one or more methylomes using the following argument:

```text
  -m,--methylomes TEXT ...    names of methylomes or a file with names
```

You can specify up to 40 methylomes per query. There are three ways to
specify methylomes. All of them use the `-m` or `--methylomes` argument.

1. Give the names of methylomes directly, following `-m` or `--methylomes` and
   multiple methylomes should be separated by spaces. These must all be for
   the same genome, and all must exist on the server. If any specified
   methylome names are not valid, then the query is considered invalid.
   So far we've specified methylomes this way for all examples whenever
   we did `-m SRX096520 SRX096521`.

2. A text file with one methylome name per line.  If the following are the
   contents of `methylomes.txt`:

   ```text
   SRX13262575
   SRX13262576
   SRX11728775
   SRX11728777
   DRX015009
   DRX015008
   ```

   Then these two commands are identical:

   ```console
   xfr query -g hg38 -i intervals.bed -o output.txt -m methylomes.txt
   xfr query -g hg38 -i intervals.bed -o output.txt \
       -m SRX13262575 SRX13262576 SRX11728775 SRX11728777 DRX015009 DRX015008
   ```

3. A JSON file that gives methylome names, and maps each to a user-defined
   "label" which will be used to form the column headings in the output file.
   This is for human interpretability, and is facilitated by the `select`
   command. I gathered some T-cell and B-cell methylomes from human using the
   `select` command. The `t_and_b_cells.json` file looks like this:

   ```json
   {
       "SRX10768294": "b_cell_00004",
       "SRX10768295": "b_cell_00003",
       "SRX10768296": "b_cell_00002",
       "SRX10768297": "b_cell_00001",
       "SRX12292982": "t_cell_00004",
       "SRX12292983": "t_cell_00002",
       "SRX12292984": "t_cell_00003",
       "SRX12292985": "t_cell_00001"
   }
   ```

   The pairs above are "name" and "label" of each methylome for the query. The
   name must be valid, and the label can be anything you choose. The `select`
   command has functions to automatically create a mapping like the one
   above. The methylomes are not necessarily ordered in this JSON file, but
   when the query completes, the columns in the output will be sorted by their
   label. This will also help, for example, color a PCA plot based on parts of
   the chosen labels. The reason to do this in the query command, and not
   later, is because I often combine these files, and when they get larger
   changing the header can become annoying. So I have the labels inserted as
   early as possible and after the query I do not use the `[DES]RX[0-9]+`
   names again in my analysis except to confirm details about the samples. I
   work with names I can remember.

   Note: if you do two queries with identical intervals and identical
   methylomes, but use a different set of labels, the order of columns in the
   output can differ.

   Here is an example using the above JSON file to specify methlomes:

   ```console
   xfr query -g hg38 -i cpgIslandExtUnmasked_hg38.bed3 -o output.txt \
       -m t_and_b_cells.json --scores -r 10
   ```

   The output looks like this (formatted with `column` for viewing):

   ```console
   $ head output.txt | column -t
   b_cell_00001        b_cell_00002  b_cell_00003  b_cell_00004  t_cell_00001  t_cell_00002  t_cell_00003  t_cell_00004
   chr1.10468.11240    0.814371      0.907725      0.904665      0.880435      0.737179      0.827957      0.744681      0.738947
   chr1.28735.29810    0             0.00854701    0.00763359    0.0158103     0.00700935    0.00537634    0.00869565    0.00524934
   chr1.51587.51860    0.929293      0.940217      0.882522      0.903915      0.934959      0.951456      0.926829      0.944882
   chr1.135124.135563  0.950617      0.984127      0.920168      0.96          0.886628      0.94964       0.926829      0.984293
   chr1.180998.181759  0.890125      0.871542      0.841105      0.835098      0.798095      0.811202      0.710843      0.774757
   chr1.199251.200253  0.00684932    0.0909091     0             0             0             0.0181818     0.00892857    0
   chr1.368746.370063  1             NA            NA            NA            0.807692      0.955556      0.709677      0.977273
   chr1.381172.382185  NA            NA            NA            NA            0.8           0.88          0.952381      0.741935
   chr1.491107.491546  NA            NA            NA            NA            NA            NA            NA            NA
   ```

### Genomes

All queries require that a genome be specified using this argument:

```text
  -g,--genome TEXT            name of the reference genome
```

This might seem redundant, since each methylome is associated with only one
genome. In the future we might support more than one assembly per species. But
the primary reason for requiring users to specify the genome is a
safety/consistency check between the user and the server, to prevent any wires
from getting crossed. This helps transferase multiple ways. It will also help
us with better error reporting in the future. Consider a query for that
requests methylation levels through intervals on chromosome 22 of a mouse
methylome. There is no chromosome 22 in the mouse genome, so either the
intervals are wrong or the methylome name is wrong. If the user supplies the
name of the genome, we will know which was the error.

If you do a lot of queries (using the command line app, or the R or Python
package), and you are only interested in one genome, then you are free make a
wrapper function or an alias that always specifies the same genome.

The genome you specify must have been "configured" so that a genome index is
available. You likely only need to worry about this if you are working with
your own data; there are extra details explained
[here](#making-a-genome-index).

### Reporting progress

By default the query command reports when it starts and when it has completed.
If there are errors, it will report those, including error codes returned by
the server. The log format includes the date, the time, the local host name,
the command (e.g., "query"), the process id, the severity of the message, and
the log message itself. The amount of output is controlled by these arguments:

```text
  -v,--verbose                report debug information while running
  -q,--quiet                  only report errors while running
```

### Configuration

A "configuration" must be available, but most users can ignore this option,
sticking with the defaults. If you are not using the default configuration, a
different directory for configuration using this option:

```text
  -c,--config-dir DIR         specify a config directory
```

The directory must exist, and it must contain certain files. Therefore, it
should be created using the `xfr config` command, and the configuration is
best understood by reading about `config` [here](#configuration).

Here is what you should consider if you suspect you might need to use the
`--config-dir` argument when doing a query.

- Whatever directory you specify with `--config-dir` must already have been
  configured using the `config` command. If you ran `xfr config` without
  specifying a directory (as in the quickstart above), then the configuration
  will be in the default place, and the query command will find it.

- If you are running your own server (explained [here](#your-server)), and
  also querying the public transferase server, you would need two
  configurations. Only one server can be configured in the default location,
  so you would need to use `--config-dir` when running queries for the
  other. If you use multiple configurations, I suggest using the default
  location for the public server.

- Maybe your home directory on your server or cluster has space restrictions,
  or maybe you like to keep your home directory free of application data. I
  can relate to both of these situations. Then you should do your `config`
  specifying some other directory. And you would need to specify the same
  directory with `--config-dir` when doing a query.

### Other options

The remaining options to the query command deal with "local mode" or when you
need to work without first doing a config. Both of these scenarios are only
useful if you have [your own data](#your-data).

## Selecting methylomes

Transferase has a command to help select methylomes for your analysis. Before
explaining the command, here's a recap of the motivation.

The public transferase server connects to MethBase2, and all methylomes are
named according to a SRA experiment accessions. An example is SRX096522. If
you know the paper you are interested in, then you can look up the accessions
for "experiments" and they are likely available in MethBase2. You could also
look at the methylomes from MethBase2 in the list at the UCSC Genome Browser.
But this is tedious, not a great way to start exploratory data analysis, and
even if you can remember the numbers, your labmates and collaborators never
will.

The select command runs in your terminal, just like the other commands, but
this one displays a list that you can navigate, and occupies your entire
terminal.

The following command will bring up an interface to allow you to select
methylomes based on the associated biological sample, in this case it would be
fore human:

```console
xfr select -g hg38 -o selected_methylomes.txt
```

If this command doesn't work for you, please let me know.

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
server will currently reject queries that involve more than 40 methylomes at
once.

While browsing or searching with the `select` command, extended "detailed"
information can be turned on/off with the 'd' key. The nature of the detailed
information varies substantially between data sets, based on what is
available.

The `select` command is probably best explained with screen captures or a
video. I will try to incoporate these in a future version of this document.
For now, get more details from the built-in help displayed by typing `h` and
experimenting.

**Labels** The "labels" we associate with each methylome in MethBase2, which
you can browse using `xfr select`, are only as good as the information people
upload to GEO and SRA at NCBI (or ENA or DDBJ). Just prior to writing the
first explanation for the `select` command I found methylomes labeled with the
taxonomic ids for orangutan and macaque, while the sample descriptions for the
corresponding samples indicated the samples were from chimp. I see similar
things frequently. Several times I've seen identical text pasted in for
different biological samples which simply covers all samples in the same
study. This is also not the fault of data submitters -- fields like "sample
name", "isolate", "biomaterial provider", "cell type", "tissue", "cell line",
"source name" and more are all available to use, and in some cases it can be
difficult to decide which one to use.

**Terminals** In the past I've seen issues with the select command not working
in certain terminals. I can no longer reproduce any of those problems when I
use the binary releases of transferase. If you build transferase yourself, it
is almost guaranteed to work in the terminal where you do the build. If you
have any problems, please open an issue.

**Disclaimer on labels** You are responsible for confirming all properties of
the methylomes you use. I suggest confirming details in the original papers
because even the metadata in NCBI databases has errors.

## Your data

If you have your own data, I strongly encourage you to use transferase in your
analysis. Running your own server, even on your laptop, can really be
worthwhile, and is covered in below. This section covers how to get your data
into a format that transferase understands.

The starting point depends on how you processed your data initially. For each
of your methylomes, you need a file that has the methylation level for each
symmetric CpG in the reference genome. The steps below assume that starting
point is data that came from
[dnmtools](https://github.com/smithlabcode/dnmtools), which was developed by
members of my lab over many years. Most data processing pipelines produce
files that can be easily adjusted to work with transferase (e.g., nanopolish,
which I used previously for nanopore DNA methylation analysis), and I will
update these instructions with other examples.

### Making a genome index

If you are working with your own data, make a genome index from the *exact*
reference genome you used to map your reads. It's a bad idea to assume that
the genome index files downloaded with `xfr config` will correspond to the
exact same reference genome you used. A difference of even a few bp, or the
inclusion of some different sequence (unassembled, alternate haplotype, etc.),
could make the results very wrong. The computing for this step should be
quick: 10 seconds for a human genome on my laptop, 20s if the genome was gzip
compressed.

To make the index, you need the reference genome in a single fasta format
file. If your reference file name is `hg38.fa`, in your current directory,
then do this:

```console
xfr index --genome hg38.fa --index-dir my_indexes
```

This command will create two files, and they should be kept together.  Any
`xfr` commands that need you to specify a genome index will ask for the
directory and genome name, so you won't be specifying exact names of these
files. But I'll explain a bit. The first one is the genome index data, which
would be named `my_indexes/hg38.cpg_idx` if you used the above command. It is
binary, and if `hg38.fa` is roughly 3.0GB, then this file will is about 113MB
in size. The other file is the genome index metadata, in JSON format. It would
be named `my_indexes/hg38.cpg_idx.json`. You can view it with any JSON viewer,
for example `cat my_indexes/hg38.cpg_idx.json | jq` would make it
readable. But there is no reason to read it.

After you made an index file (or multiple), you can see the available genomes
for use with transferase like this:

```console
xfr list my_indexes
```

It will show you the names of genomes with indexes in the specified directory.

If you are working through these steps to learn the process, and not with your
own data, the steps below will require that you use the reference genome file
I used to map reads:

* [hg38.fa.gz](https://smithlabresearch.org/transferase/other/hg38.fa.gz)
  (826M) Might take up to 5 min to download.

```console
wget https://smithlabresearch.org/transferase/other/hg38.fa.gz
```

### Formatting methylomes

The starting point to make a methylome file with single-nucleotide methylation
levels. These must be in terms of counts, for the CpG sites. These are assumed
to be for the "symmetric" CpG sites. They must be indexed from 0, and
reference only the `C` of the CpG.  Several commands in `dnmtools` generate
files like this from mapped reads (e.g., BAM format). If you are uncertain
about this, please see the documentation for the `counts` dnmtools command
[here](https://dnmtools.readthedocs.io/en/latest/counts).

Here are the example files I'm using:

* [SRX9531739.xsym.gz](https://smithlabresearch.org/transferase/other/SRX9531739.xsym.gz) (70M)
* [SRX9531751.sym.gz](https://smithlabresearch.org/transferase/other/SRX9531751.sym.gz) (142M)

```console
wget https://smithlabresearch.org/transferase/other/SRX9531739.xsym.gz \
    https://smithlabresearch.org/transferase/other/SRX9531751.sym.gz
```

Both are gzip compressed. The file with extension `.xsym.gz` is in "xcounts"
format and the other is in
[counts](https://dnmtools.readthedocs.io/en/latest/counts) format. Both encode
methylation levels at each symmetric CpG site in the human genome for the
given methylomes. Both of these formats are produced by dnmtools, and clearly
one is smaller than the other.

If you are using the example files, then be sure you made a genome index for
the `hg38.fa.gz` reference genome downloaded from the link in the previous
step. As I might have said already, the genome index must exactly match the
reference genome used to map reads. The format command generates methylomes
in the format needed by transferase:

```console
xfr format -g hg38 -x my_indexes -d my_methylomes -m SRX9531739.xsym.gz
```

The arguments above are the name of the genome, the directory where the genome
index is located, a directory to put the methylomes, and the input file with
methylation levels.

Be careful about the `-x` argument, because if you forget it, then `xfr` might
find a genome index with the same name in your default config directory, which
would have been created if you did the "quickstart" at the top of this file.

Similar to the genome index, a methylome in transferase format involves two
files. They will both be placed in the output directory (`my_methylomes`) and
whenever they are used they will be used together. Again, as with the genome
index, one file is binary and the other is JSON. The only reason to ever read
the JSON file is if you lost track of where a methylome came from or how it
was produced.

After you made a transferase format methylome with the output directory
`my_methylomes` you can use the `list` command to see the methylomes available
in that directory:

```console
xfr list my_methylomes
```

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

### Running the query command locally

As with the remote query, you need query intervals. You can reuse the same
ones. If you named the directories `my_indexes` and `my_methylomes` in earlier
steps, and used all the example data files, then this is the command:

```console
xfr query --local \
    -g hg38 -i cpgIslandExtUnmasked_hg38.bed3 -o output.txt -m SRX9531739 \
    -x my_indexes -d my_methylomes
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

The query command can be run in "local mode" which does not communicate with a
server, and instead reads methylome information directly from the local
filesystem.  Even if you are analyzing methylomes that are on your local
filesystem, starting your own transferase server (e.g., on your laptop) should
be as efficient in nearly all situations, more efficient in many situations,
and usually more convenient. You turn on local mode like this:

```text
  -L,--local                  run in local mode
```

Running in local mode requires your methylomes be on the local machine, be
properly formatted (see the `format` command) and reside in a directory given
with the `--methylome-dir` option. Local mode also requires that you specify a
directory with genome index files (see the `index` command).

```text
  -d,--methylome-dir DIR      methylome directory to use in local mode
  -x,--index-dir DIR          genome index directory
```

Important: If you are running in local mode, likely you are using your own
methylomes that you mapped yourself. If you mapped the reads, then you need to
make your own genome index from the reference genome used for mapping. Do not
copy existing genome index files and assume they are correct. If you
co-analyze data from the public transferase server alongside your own private
data, first create your own genome index, then verify that the hashes are
identical. You  can do this with the `check` command.

## Your server

If you were able to work through the commands in the previous section to use
your own data, then running your own server will be trivial. It can be much
faster and it's really light-weight. If your DNA methylation project can
benefit from exploratory data analysis, you should do this. If you have an 8GB
laptop, you can easily keep 50 methylomes live at the same time, which makes
queries responses essentially instantaneous.

The `server` command can be run locally, on your laptop, or you can run it
from a server or workstation in your lab. The easiest way to start is to run
the server on your own laptop or desktop machine. Just open two terminal
windows: one for the server and the other to do your data analysis.

The steps here assume you have the genome index for hg38 and a methylome named
SRX9531751, both from the download links above. If you have some other genome
or methylome, it's fine, just substitute the names.

### Starting your server

For this example, your methylomes should be in a directory named
`my_methylomes` and your genome indexes should be in `my_indexes`. The
`my_indexes` directory should contain genome indexes for each genome
associated with a methylome in `my_methylomes`.

Using names and data from the above examples, we would have a single index and
two methylomes. Here is a command to start the server:

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

When you need to stop the server, use ctrl-c or close the window -- however
you do it is fine.

### Querying your local server

The query command works the same way for your server as it does for the public
server and the instructions for the query command above apply for your server
as well. But configuring to easily query your own server requires that you
specify some values, since the defaults won't apply.

Assuming you started the server in a different terminal window as explained
above, you will need to generate a configuration to query it. This command
will do the configuration:

```console
xfr config -c my_config -s localhost -p 5000 \
    -x /path/to/my_indexes --download none
```

If you are still using the same example directories from earlier commands, you
should have a genome index for hg38 already in the `my_indexes` directory.
Notice that when configuring the client, above, the full path was given to the
`my_indexes` directory. If the full path was not given, then the path would be
assumed to be inside the `my_config` directory. If you are still working in
the directory that contains `my_indexes`, you can specify it on the command
line like `-x $(pwd)/my_indexes ...`

With this configuration, you can do a query almost exactly as if you were
querying MethBase2 on the transferase server:

```console
xfr query -c my_config -g hg38 -i cpgIslandExtUnmasked_hg38.bed3 \
    -o output.txt -m SRX9531739
```

If you want to confirm that everything works, compare the output from running
the query command remotely vs. locally and check that the output is the same
for the example methylomes provided via download links above.

If you made it this far with your own server, check out the server
documentation (as of now, in a file named `server.md`), which includes
information about how to optimize performance.

### Queries with no configuration

It is possible to run `xfr query` without any configuration, but it's not
convenient. The only reason to do it is if you are testing a server you are
setting up in your lab. Running without a configuration directory requires
that you specify the server hostname or IP address, the port and a local
directory with genome index files. These are the options you would use to the
query command to directly specify the information that would otherwise be
taken from the configuration:

```text
  -s,--hostname TEXT          server hostname or IP address
  -p,--port INT               server port
  -x,--index-dir DIR          genome index directory
```

Here is an example of what a query command might look like without using a
configuration:

```console
xfr query -s powerlevel.edu -p 9001 -x index_dir \
    -g hg38 -o output.txt -i intervals.bed -m methylomes.txt
```
