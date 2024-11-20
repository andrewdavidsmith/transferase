# mxe
Methylome transfer system

# Usage

There are several commands within `mxe` and the best way to start is
to understand the `dnmtools roi` command, as the information
functionality provided by `mxe` is the same.

## Make an index file

Before starting, an index file is required. To make the index, you
need the reference genome in a single fasta format file. If your
reference file name is `hg38.fa`, then do this:
```console
mxe index -v -g hg38.fa -x hg38.cpg_idx
```
If `hg38.fa` is roughly 3.0G in size, then you should expect
`hg38.cpg_idx` to be about 113M in size.

## Make a methylome file

The starting point within `mxe` is a file in the `xcounts` format that
involves the symmetric CpG sites. This is called a symmetric xcounts
format file, and may be gzip compressed, in which case the extension
should be `.xsym.gz`. First I will explain how to start with this
format, and below I will explain how to start with the other format
used by `dnmtools`. We again assume that the reference genome is
`hg38.fa` and that this file was used to create the
`SRX012345.xsym.gz` file. This ensures a correspondence between the
`SRX012345.xsym.gz` file and the `hg38.cpg_idx` file we will need.
Here is how to convert such a file into the mxe format:

```console
mxe compress -v -x hg38.cpg_idx -m SRX012345.xsym.gz -o SRX012345.m16
```

If the chromosomes appear out-of-order in `hg38.cpg_idx` and
`SRX012345.xsym.gz` an error will be reported.

If you begin with a counts format file, for example `SRX012345.sym`,
created using the `dnmtools counts` and then `dnmtools sym` commands,
then you will need to first convert it into `.xsym` format. You can do
this as follows:
```console
dnmtools xcounts -z -o SRX012345.xsym.gz -c hg38.fa -v SRX012345.sym
```
Once again, be sure to always use the same `hg38.fa` file.  Strictly,
what matters for the `hg38.fa`, or any other reference genome, is that
the order and lengths or chromosomes are identical.

## Run the `lookup` command locally

This step is to make sure everything is sensible. You will need a set
of genomic intervals of interest. In this example these will be named
`intervals.bed`. You also need the index file and the methylome file
explained in the above steps.
```console
mxe lookup local --log-level debug -x hg38.cpg_idx -m SRX012345.m16 -o intervals.bed.info -i intervals.bed
```
The index file `hg38.cpg_idx` and the methylome file `SRX012345.m16`
are the same as explained above. At the time of writing, the
`intervals.bed` file must be 6-column bed format, so if yours is only
3 columns, you can use a simple awk command to pad it out as follows:
```console
awk -v OFS="\t" '{print $1,$2,$3,"X",0,"+"}' intervals.bed3 > intervals.bed
```

The output in the `intervals.bed.info` file should be consistent with
the information in the command:
```console
dnmtools roi -o intervals.roi intervals.bed SRX012345.xsym.gz
```
The format of the output might be different, but the methylation
levels on each line (i.e., for each interval) should be identical.

## Run the `lookup` command remotely

We will assume for now that the hostname of the remote `mxe` server is
example.com, and the port is 5000. The following command should give
identical output to the above command:
```console
mxe lookup remote --log-level debug -x hg38.cpg_idx -m SRX012345 -o intervals.bed.info -i intervals.bed
```

Note that `SRX012345` now is not a file, but rather a methylome name,
or accession, that is available on the server. If it is not, the
server will respond that the accession is not available.
