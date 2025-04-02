# MIT License
#
# Copyright (c) 2025 Andrew Smith
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# These tests come from commands in a version of the transferase Python API
# example usage docs. Before running these, get the file in the current dir as
# follows:
# wget https://smithlabresearch.org/transferase/other/cpgIslandExtUnmasked_hg38.bed3

# Declare variables to use in the query
methylome_names = ["ERX9474770","ERX9474769"]
genome_name = "hg38"
intervals_filename = "cpgIslandExtUnmasked_hg38.bed3"
bin_size = 100000

# Set the log level to debug to see all
import pyxfr
from pyxfr import LogLevel
pyxfr.set_log_level(LogLevel.debug)

# Do the config
from pyxfr import MConfig
config = MConfig()
config.install([genome_name])

# Get a client
from pyxfr import MClient
client = MClient()

# Load the genome index
from pyxfr import GenomeIndex
genome_index = GenomeIndex.read(client.get_index_dir(), genome_name)

# Read the genomic intervals for the query
from pyxfr import GenomicInterval
intervals = GenomicInterval.read(genome_index, intervals_filename)

# Do the query using the intervals
levels = client.get_levels(methylome_names, intervals)

print("\n".join([str(levels.at(i, 1)) for i in range(10)]))

query = genome_index.make_query(intervals)
levels = client.get_levels_covered(methylome_names, query)

for j in range(levels.n_methylomes):
    for i in range(10):
        print(intervals[i].to_string(genome_index), levels.at(i, j))
    print()

arr = levels.view_nparray()
print("levels as nparray:")
print(arr[0:10])
print()

min_reads = 2
means = levels.all_wmeans(min_reads)
print("levels as weighted means:")
print(means[0:10])
print()

## Get the number of CpGs in query regions
print("n_cpgs intervals:")
n_cpgs_intervals = genome_index.get_n_cpgs(intervals)
print("\n".join([str(i) for i in n_cpgs_intervals[0:10]]))
print()

print("n_cpgs bins:")
n_cpgs_bins = genome_index.get_n_cpgs(bin_size)
print("\n".join([str(i) for i in n_cpgs_bins[0:10]]))
print()
