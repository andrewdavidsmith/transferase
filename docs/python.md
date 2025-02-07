# Python usage examples

This document is the first shot at very rudimentary usage docs for the
Python API. These are intended for people who are testing the
software. I expect this document to evolve and then disappear as real
documentation is written for the transferase Python API. This API is
entirely through C++ bindings, so any `.py` files in might find in the
repo are not part of the API, and would just be for the build process.

First we import the transferase module so we can set our preferred log
level for the session. Setting it to 'debug' let's us see
everything. It will be a lot of information, most actually for
debugging.

```python
import transferase
from transferase import LogLevel

transferase.set_log_level(LogLevel.debug)
```

Next we import the MethylomeClient class. This is our interface to the
transferase server. We need to use the class to set up our
environment.

```python
from transferase import MethylomeClient
```

We next configure transferase and install any genomes we will use. We
will use the default configuration and ask for hg38, but first we will
produce an error (you don't need the try block -- feel free to watch
it crash):

```python
try:
    MethylomeClient.config(["hg399"])
except RuntimeError as err:
    print(f"Error: {err}")
```

This should succeed as long as the server is responding and has the
files for 'hg38', which it should.

```python
MethylomeClient.config(["hg38"])
```

With the configuration finished, we can instantiate a client.

```python
client = MethylomeClient.init()
```

The client allows us to make queries. Our query will be based on a set
of genomic intervals, as usually stored in a BED format file. However,
before working with the genomic intervals we need to first load a
genome index, which guarantees that we are working with the exact same
reference genome as the transferase server we will contact.

```python
from transferase import GenomeIndex
```

This should fail because the genome doesn't exist:

```python
try:
    genome_index = GenomeIndex.read(client.index_dir, "hg399")
except RuntimeError as err:
    print(f"Error: {err}")
```

But this should succeed because we configured 'hg38':

```python
genome_index = GenomeIndex.read(client.index_dir, "hg38")
```

We will now read genomic intervals. If you have a BED format file for
hg38, for example ~100k intervals (you can do up to over 1M, but it
will be slower), you can use it. Otherwise you can find the
'intervals.bed.gz' in the docs directory of the repo, unpack it and
put it in your working directory.

```python
from transferase import GenomicInterval
intervals = GenomicInterval.read(genome_index, "intervals.bed")
```

From this we make the query object.

```python
query = genome_index.make_query(intervals)
```

The following should fail because a list is expected as the argument
type to 'get_levels':

```python
try:
    levels = client.get_levels("ERX9474770,ERX9474769", query)
except TypeError as err:
    print(err)
```

The following should fail because a the name format is invalid; it
should be alphanumeric, possibly with underscore:

```python
try:
    levels = client.get_levels(["ERX9474770,ERX9474769"], query)
except RuntimeError as err:
    print(f"Error: {err}")
```

And this should fail because the methylome doesn't exist:

```python
try:
    levels = client.get_levels(["asdf"], query)
except RuntimeError as err:
    print(f"Error: {err}")
```

But this should succeed:

```python
levels = client.get_levels(["ERX9474770", "ERX9474769"], query)
```

And now the see the methylation levels that were just retrieved, just
the first 10 intervals for each of the two methylomes in our query:

```python
for methylome_results in levels:
    for i in range(10):
        print(methylome_results[i])
    print()
```

This will show us the levels along with the original intervals, where
the `to_string` function retrieves chromosome names:

```python
for methylome_results in levels:
    for i in range(10):
        print(intervals[i].to_string(genome_index), methylome_results[i])
    print()
```
