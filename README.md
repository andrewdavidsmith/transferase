# xfrase
The transferase system for querying methylomes in methbase.

The transferase system has the alias `xferase` which is quicker to
type.  There are several commands within `xfrase` and the best way to
start is to understand the `dnmtools roi` command, as the information
functionality provided by `xfrase` is the same. If you need to learn
about `dnmtools roi` you can find the docs
[here](https://dnmtools.readthedocs.io/en/latest/roi/)

## Make an index file

Before starting, an index file is required. To make the index, you
need the reference genome in a single fasta format file. If your
reference file name is `hg38.fa`, then do this:
```console
xfrase index -v -g hg38.fa -x hg38.cpg_idx
```
If `hg38.fa` is roughly 3.0G in size, then you should expect the index
file `hg38.cpg_idx` to be about 113M in size. This command will also
create an "index metadata" file `hg38.cpg_idx.json`, which is named by
just adding the `.json` extension to the provided output file.

## Make a methylome file

The starting point within `xfrase` is a file in the `xcounts` format that
involves the symmetric CpG sites. This is called a symmetric xcounts
format file, and may be gzip compressed, in which case the extension
should be `.xsym.gz`. First I will explain how to start with this
format, and below I will explain how to start with the other format
used by `dnmtools`. We again assume that the reference genome is
`hg38.fa` and that this file was used to create the
`SRX012345.xsym.gz` file. This ensures a correspondence between the
`SRX012345.xsym.gz` file and the `hg38.cpg_idx` file we will need.
Here is how to convert such a file into the xfrase format:

```console
xfrase compress -v -x hg38.cpg_idx -m SRX012345.xsym.gz -o SRX012345.m16
```

If the chromosomes appear out-of-order in `hg38.cpg_idx` and
`SRX012345.xsym.gz` an error will be reported. As with the `hg38.cpg_idx`
index file, the methylome file `SRX012345.m16` will be accompanied by
a metadata file with an additional json extension: `SRX012345.m16.json`.

If you begin with a counts format file, for example `SRX012345.sym`,
created using the `dnmtools counts` and then `dnmtools sym` commands,
then you will need to first convert it into `.xsym` format. You can do
this as follows:
```console
dnmtools xcounts -z -o SRX012345.xsym.gz -c hg38.fa -v SRX012345.sym
```
Once again, be sure to always use the same `hg38.fa` file.  A hash
function will be used internally to `xfrase` to ensure that the index
and methylome files correspond to the same reference genome file.

## Run the `lookup` command locally

This step is to make sure everything is sensible. You will need a set
of genomic intervals of interest. In this example these will be named
`intervals.bed`. You also need the index file and the methylome file
explained in the above steps.
```console
xfrase lookup local --log-level debug -x hg38.cpg_idx -m SRX012345.m16 -o intervals_local_output.bed -i intervals.bed
```
The index file `hg38.cpg_idx` and the methylome file `SRX012345.m16`
are the same as explained above. At the time of writing, the
`intervals.bed` file must be 6-column bed format, so if yours is only
3 columns, you can use a simple awk command to pad it out as follows:
```console
awk -v OFS="\t" '{print $1,$2,$3,"X",0,"+"}' intervals.bed3 > intervals.bed
```

The output in the `intervals_local_output.bed` file should be
consistent with the information in the command:

```console
dnmtools roi -o intervals.roi intervals.bed SRX012345.xsym.gz
```
The format of the output might be different, but the methylation
levels on each line (i.e., for each interval) should be identical.

## Run the `server` command

The `server` command can be tested first locally by using two terminal
windows. We require the index file `hg38.cpg_index` and the methylome
file `SRX012345.m16` along with the corresponding `.json` metadata
files. These should be in directories named for methylomes and
indexes. The methylomes directory will contain all files with `.m16`
and `.m16.json` extensions; these are the methylomes that can be
served. The indexes directory will contain the files ending with
`.cpg_idx` and `.cpg_idx.json`; these are index files for each
relevant genome assembly. For now, using the above examples, we
would have a single index and a single methylome. I will assume these
are in subdirectories, named `indexes` and `methylomes` respectively,
of the current directory. Here is a command that will start the server:

```console
xfrase server -v debug -s localhost -p 5000 -m methylomes -x indexes
```
Not that this will fail with an appropriate error message if port 5000
is already be in use, and you can just try 5001, etc., until one is
free. The `-v debug` will ensure you see info beyond just the errors. This
informtion is logged to the terminal by default.

## Run the `lookup` command remotely

We will assume for now that "remote" server is running on the local
machine (localhost) and using port is 5000 (the default). The
following command should give identical earlier `lookup` command:

```console
xfrase lookup remote -v debug  -s localhost -x indexes/hg38.cpg_idx \
    -o intervals_remote_outout.bed -a SRX012345 -i intervals.bed
```
Note that now `SRX012345` is not a file this time. Rather, it is a
methylome name or accession, and should be available on the server. If
the server can't find the named methylome, it will respond indicating
that methylome is not available.
