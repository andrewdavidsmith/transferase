# Transferase command line tools for data analysis

The transferase program has an alias `xfr` (installed as just a copy
of the program) which is quicker to type and will be used below. There
are several commands within `xfr`. The best way to understand what
these commands do is to understand the `dnmtools roi` roi tool. The
information provided by transferase is the same as what people in the
Smith lab used to get with `dnmtools roi`. You can find the
documentation
[here](https://dnmtools.readthedocs.io/en/latest/roi).

## Configuring transferase

Before starting, you should do a quick configuration. This makes
everything simpler in later steps. Here is the command:

```console
xfr config -g hg38,mm39
```

This command will give you a default configuration in a default
place. It will download (regular web download) some information for
the genomes hg38 and mm39, which are the assemblies used for human and
mouse in MethBase2. The amount of data downloaded for this step is
about 200MB and might take about 30 seconds, depending on your
internet speed. If you are interested in other species, check out
which ones are available through MethBase2.

## Selecting methylomes

The methylomes in MethBase2 are all "named" according to a SRA
accession number. The study "SRP08344", from Hodges et al. (2011)
included methylomes for 4 types of blood cells, with SRX096522 and
SRX096523 being two technical replicates from pooled blood of multiple
people. These are both in MethBase2 as individual methylomes, but the
names SRX096522 and SRX096523 are not easy to remember. If you know
the paper, you can look up the accessions for "experiments" and they
are likely available in MethBase2. You could also look at the
methylomes from MethBase2 in the list at the UCSC Genome Browser.

Note: the "labels" we associate with each methylome are only as good
as the information people upload to GEO and SRA at NCBI. Just prior to
writing this I found methylomes labeled with the taxonomic ids for
orangutan and macaque, while the sample descriptions that the samples
were taken from chimp. Just be aware.

But transferase comes with tool that can help you get started. The
following command will bring up an interface to allow you to select
methylomes based on the associated biological sample, in this case
it would be fore human:

```console
xfr select -g hg38 -o selected_samples.txt
```

You will be asked to confirm before proceeding, and then you will see
a list of methylomes. For human (hg38) there will be a long list of
roughly 5,500 methylomes (as of Feb 1, 2025), each with some
information about the biological sample. You can navigate the list
with the arrow keys, page-up/down, home and end. You can add a
methylome with the space bar, or add/remove multiple at once by
switching modes (there is a legend at the top of the
screen). Importantly, you can search the list, and then select
methylomes matching your query. The search is not case-sensitive, but
"b cell" and "b-cell" will not be considered the same. When you quit
your selection will be saved in `selected_samples.txt` or whatever
output file you specified.

To proceed, either select several methylomes this way, or some other
way. Don't try to select all samples -- the server will not allow you
to try and download the entire database.

## Doing a query

You will need some intervals. If you do any genomics, you probably
have several different files that list genomic regions of interest to
you -- and in BED format, maybe 3 or 6 column. If you don't, you can
download some [here](the link).

```console
xfr query -g hg38 -m SRX096522 -i hg38_promoters.bed -o the_results.txt
```

# Working with your own data

## Make an index file

Before starting, an index file is required. To make the index, you
need the reference genome in a single fasta format file. If your
reference file name is `hg38.fa`, then do this:
```console
xfr index -v -g hg38.fa -x hg38.cpg_idx
```

If `hg38.fa` is roughly 3.0G in size, then you should expect the index
file `hg38.cpg_idx` to be about 113M in size. This command will also
create an "index metadata" file `hg38.cpg_idx.json`, which is named by
just adding the `.json` extension to the provided output file. You can
also start with a gzip format file like `hg38.fa.gz`.

## Make a methylome file

The starting point is a file in the `xcounts` format that involves the
symmetric CpG sites. This is called a symmetric xcounts format file,
and may be gzip compressed, in which case the extension should be
`.xsym.gz`. First I will explain how to start with this format, and
below I will explain how to start with the other format used by
`dnmtools`. We again assume that the reference genome is `hg38.fa` and
that this file was used to create the `SRX012345.xsym.gz` file. This
ensures a correspondence between the `SRX012345.xsym.gz` file and the
genome index (two files named `hg38.cpg_idx` and `hg38.cpg_idx.json`).
Here is how to convert such a file into the transferase format:
```console
xfr format -x index_dir -g hg38 -o methylome_dir -m SRX012345.xsym.gz
```
As with the genome index, the methylome in transferase format will
involve two files: `SRX012345.m16` and `SRX012345.m16.json`, the
latter containing metadata.

If you begin with a
[counts](https://dnmtools.readthedocs.io/en/latest/counts) format
file, for example `SRX012345.sym`, created using the `dnmtools counts`
and then `dnmtools sym` commands, then you will need to first convert
it into `.xsym` format (whether gzipped or not). You can do this as
follows:
```console
dnmtools xcounts -z -o SRX012345.xsym.gz -c hg38.fa -v SRX012345.sym
```

Once again, be sure to always use the same `hg38.fa` file.  A hash is
generated and used internally to transferase to ensure that the index
and methylome files correspond to the same reference genome file.

### Run the query command locally

This step is to make sure everything is sensible. Or you might just
want to keep using this tool for your own analysis (it can be
extremely fast). We will assume first that you have a set of genomic
intervals of interest. In this example these will be named
`intervals.bed`. You also need the genome index and the methylome
generated in the above steps.
```console
xfr query --local \
    -x index_dir -g hg38 -d methylome_dir -m SRX012345 \
    -o output.bed -i intervals.bed
```

The genome index for hg38 and the methylome for SRX012345 are the same
as explained above (each is two files). The `intervals.bed` file may
contain any number of columns, but the first 3 columns must be in
3-column BED format: chrom, start, stop for each interval.  The output
in the `local_output.bed` file should be consistent with the
information in the command:
```console
dnmtools roi -o intervals.roi intervals.bed SRX012345.xsym.gz
```

The format of these output files are different, but the methylation
levels on each line (i.e., for each interval) should be identical.
Note that `dnmtools roi` can fail if intervals are nested, while
`xfr query` command will still work.

### Running a transferase server

The `server` command can be tested first locally by using two terminal
windows. The steps here assume you have the genome index for hg38 and
a methylome named SRX012345. If you have some other methylome it's
fine, just substitute the name. These files should be in directories
named `methylomes` and `indexes` for this example. The methylome
directory will contain files with `.m16` and `.m16.json` extensions;
these correspond to the methylomes that can be served and must be in
pairs. For every `.m16` file there must be a `.m16.json` file, and
vice-versa. The indexes directory will contain the files ending with
`.cpg_idx` and `.cpg_idx.json`; these are index files for each
relevant genome assembly. For now, using the above examples, we would
have a single index and a single methylome. Here is a command that
will start the server:

```console
xfr server -v debug -s localhost -p 5000 -m methylomes -x indexes
```

Above, the arguments indicate to use the host localhost with
port 5000. Note that this will fail with an appropriate error message
if port 5000 is already be in use, and you can just try 5001, etc.,
until one is free. The hostname can also be specified numerically (in
this example, 127.0.0.1). The `-v debug` will ensure you see info
beyond just the errors. This informtion is logged to the terminal by
default. The server can also be run in detached mode, but if you don't
know what that means, you don't likely have any reason to do it.

### Run the query command remotely

We will assume for now that "remote" server is running on the local
machine (localhost) and using port is 5000 (the default). You would
have started this in the previous step using a different terminal
window. The following command should give identical results to the
earlier `xfr query` command:

```console
xfr query -v debug -s localhost -x indexes \
    -o remote_output.bed -m SRX012345 -i intervals.bed
```

Note that now `SRX012345` is not a file. Rather, it is a methylome
name, and should be available on the server. If the server can't find
the named methylome, it will respond indicating that methylome is not
available.
