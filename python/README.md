# pyxfr

The pyxfr package is an API for transferase. Learn more about transferase
[here](https://github.com/andrewdavidsmith/transferase). This package allows
the same queries to be done within Python as with the transferase command line
app. Almost all other utilities for manipulating transferase data are
available through pyxfr.

Although documentation is still sparse for pyxfr, each class and function in
pyxfr has built-in documentation:

```python
from pyxfr import pyxfr, pyxfr_utils
help(pyxfr)
help(pyxfr_utils)
```

## Requirements

- Linux: Python >= 3.12
- macOS: Python >= 3.12 and macOS >= 13 (at least Ventura)

## Usage examples

First we import the pyxfr module so we can set our preferred log level for the
session or in your Python scripts. Setting it to "debug" let's us see
everything. It will be a lot of information, most of it actually for
debugging.

```python
import pyxfr
from pyxfr import LogLevel
pyxfr.set_log_level(LogLevel.debug)
```

Next we want to set up transferase for the user (i.e., your login account) on
the host system (e.g., your laptop). The following will do a default setup,
and might take up to a minute:

```python
from pyxfr import MConfig
config = MConfig()
config.install(["hg38"])
```

This will create files in `~/.config/transferase` which are safe to delete
anytime because you can just run the same command again. The reason this is
done in two steps is because you might want to change something in the
`config` before doing the installation. By typing the name of the variable
`config` you will see a dump of its values. For now, leave them as their
default values -- they only need to be changed if you are using local data.

You can select other genomes in the `install` step (e.g., mm39, rn7, bosTau9,
etc.). If the genomes don't exist or are not on the server, you should see a
`RuntimeError` exception in Python, indicating a problem downloading. The
server can't tell the difference between an invalid genome assembly name, one
that is misspelled, and a real one that simply isn't on the server. You can
find the list of available genomes by checking out MethBase2 through the UCSC
Genome Browser, or using a command I will show below.

With the setup has completed, we can get a client object:

```python
from pyxfr import MClient
client = MClient()
```

The client object is what makes the queries. Our query will be based on a set
of genomic intervals, which you would get from a BED format file. However,
before working with the genomic intervals we need to first load a genome
index, which guarantees that we are working with the exact reference genome
that the transferase server expects.

```python
from pyxfr import GenomeIndex
genome_index = GenomeIndex.read(client.get_index_dir(), "hg38")
```

We will now read genomic intervals. If you have a BED format file for hg38,
for example around 100k intervals, you can use it. Otherwise you can find the
[`intervals.bed.gz`](https://github.com/andrewdavidsmith/transferase/blob/main/docs/intervals.bed.gz)
in the docs directory of the repo (likely alongside this file), gunzip it and
put it in your working directory.

```python
from pyxfr import GenomicInterval
intervals = GenomicInterval.read(genome_index, "intervals.bed")
```

At this point we can do a query:

```python
genome_name = "hg38"
methylome_names = ["ERX9474770","ERX9474769"]
levels = client.get_levels(genome_name, methylome_names, intervals)
```

The `levels` is an object of class `MLevels`, which is a matrix where rows
correspond to intervals and columns correspond to methylomes in the query.

The following loop will allow us to see the first 10 methylation levels that
were retrieved for the second (`ERX9474769`) of the two methylomes in our
query:

```python
print("\n".join([str(levels.at(i, 1)) for i in range(10)]))
```

It should look like this:

```console
(4, 1)
(1, 471)
(0, 0)
(45, 29)
(0, 0)
(334, 346)
(62, 1581)
(51, 1755)
(74, 664)
(199, 1753)
```

If you are doing multiple queries that involve the same set of genomic
intervals, it's more efficient to make a `MQuery` object out of them. This is
done internally by the query above, but the work to do it can be skipped if
they you already have them. Here is an example:

```python
genome_name = "hg38"
methylome_names = ["ERX9474770","ERX9474769"]
query = genome_index.make_query(intervals)
levels = client.get_levels_covered(genome_name, methylome_names, query)
```

If you want to see the methylation levels alongside the original genomic
intervals, including printing the chromosome names for each genomic interval,
you can do it as follows:

```python
for col in range(levels.n_methylomes):
    for row in range(10):
        print(intervals[row].to_string(genome_index), levels.at(row, col))
    print()
```

The `at` function for class `MLevels` will return a tuple of
`(n_meth,n_unmeth)` and if the `_covered` version of the query was used, it
will return a tuple of `(n_meth,n_unmeth,n_covered)`. These create the tuple
objects that they return Python. If you want the corresponding values without
creating the tuple, you can get them with the `get_n_meth(i,j)`,
`get_n_unmeth(i,j)` and `get_n_covered(i,j)` functions, where `i` is the
row/interval, and `j` is the column/methylome. Here is an example:

```python
for i in range(10):  # 10 for brevity
    for j in range(levels.n_methylomes):
        print(levels.get_n_covered(i, j), end=' ')
    print()
```

The `get_n_covered(i,j)` will raise an exception if the information about
covered sites was not requested in the query. There is also a `get_wmean(i,j)`
function that can be applied similarly, which gives the weighted mean
methylation level for the corresponding interval. This is likely the most
useful summary statistic in all of methylome analysis.

You can convert an `MLevels` object into a numpy array, which is already
familiar to many Python users. The following commands assume that the `levels`
object was obtained from a `get_levels_covered` query, hence the `3` as the
final dimension in `a.shape`:

```python
a = levels.view_nparray()
methylome_names = ["ERX9474770","ERX9474769"]
assert a.shape == (len(methylome_names), len(intervals), 3)
```

The `intervals` in that command came from an earlier command above. The full
set of weighted mean methylation levels can be obtained as a numpy array
(matrix) using the `all_wmeans(min_reads)` function, applied to the entire
`levels` object. The `min_reads` parameter indicates a value below which the
fraction is not interpretable. This is needed, because the information about
whether there are even any reads at all for a given interval would be
lost. Entries without enough reads are assigned a value of `-1.0`. Here is an
example (the output is a numpy array):

```python
min_reads = 2
means = levels.all_wmeans(min_reads)
print(means[0:10])
```

As we have seen, the queries named with `_covered` return, along with
methylation levels, the number of CpG sites covered by reads in each query
interval. It's also helpful to know how many total CpG sites are in the
interval. This is a property of the reference genome, not a particular
methylome. If you have a set of intervals and a GenomeIndex, you can get the
total number of CpG sites in each interval like this:

```python
n_cpgs_intervals = genome_index.get_n_cpgs(intervals)
print("\n".join([str(i) for i in n_cpgs_intervals[0:10]]))
```

And an addition in v0.6.1: you can get the metadata for MethBase2 methylomes
available on the public transferase server. Here is the command and along with
what you might expect to see:

```python
methbase_metadata = pyxfr_utils.load_methbase_metadata("hg38")
sum(methbase_metadata.sample_name == "Sperm")
# 174
list(methbase_metadata.columns.values)
# ['study', 'experiment', 'bsrate', 'meth', 'depth', 'mapping', 'uniq',...
```
