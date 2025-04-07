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

"""
Utilities associated with pyxfr, the Python API for Transferase, but that
are not implemented within the pyxfr C++ bindings.
"""

import json
import os

from .pyxfr import GenomeIndex

try:
    import pandas

    pandas_available = True
except ImportError:
    pandas_available = False


def generate_bins(genome_index, bin_size):
    """
    Generate bins as triples (tuples) of (chrom, start, stop) values with the
    final bin typically not being the entire bin size.

    Parameters
    ----------

    genome_index (GenomeIndex): A GenomeIndex object, which defines chromosome
    names and sizes.

    bin_size (int): The size of bins to generate.
    """
    try:
        index_as_dict = json.loads(str(genome_index))
    except json.JSONDecodeError as err:
        raise Exception("Failed to extract metadata from genome index")
    if not isinstance(index_as_dict, dict) or "meta" not in index_as_dict:
        raise Exception("Failed to identify 'meta' in genome index")
    # ADS: if we get here, we assume for now that everything is ok
    meta = index_as_dict["meta"]
    chrom_names = [
        i[-1]
        for i in sorted([(v, k) for k, v in meta["chrom_index"].items()])
    ]
    chrom_sizes = dict(zip(chrom_names, meta["chrom_size"]))
    for chrom_name, chrom_size in chrom_sizes.items():
        start = 0
        stop = 0
        while start < chrom_size:
            stop = min(start + bin_size, chrom_size)
            yield (chrom_name, start, stop)
            start = stop


def load_methbase_metadata(genome, config_dir=None):
    """
    Load the MethBase2 metadata as a pandas data frame. Returns a pandas data
    frame.

    Parameters
    ----------

    genome (str): load the MethBase2 metadata for this genome assembly.
    """
    if not pandas_available:
        raise ImportError(
            "'load_methbase_metadata' requires pandas, "
            "which could not be imported"
        )
    if not config_dir:
        home_dir = os.getenv("HOME")
        if not home_dir:
            raise KeyError("failed to identify home directory")
        config_dir = os.path.join(home_dir, ".config", "transferase")
    elif not os.path.exists(config_dir):
        raise Exception(
            f"directory does not exist: {config_dir}. "
            "See documentation for MConfig"
        )
    from .pyxfr import __version__ as pyxfr_version

    metadata_filename = f"methbase_metadata_dataframe_{pyxfr_version}.tsv"
    metadata_path = os.path.join(config_dir, metadata_filename)
    if not os.path.exists(metadata_path):
        raise OSError(
            errno.ENOENT, "methbase metadata file not found", metadata_path
        )
    full_table = pandas.read_csv(metadata_path, sep="\t")
    return full_table
