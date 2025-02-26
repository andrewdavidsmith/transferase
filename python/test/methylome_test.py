# MIT License
#
# Copyright (c) 2024 Andrew D Smith
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


import pytest
import shutil
import tempfile
import os

from transferase import GenomeIndex
from transferase import Methylome
from transferase import GenomicInterval
from transferase import MLevels
from transferase import MLevelsCovered


def get_valid_test_genome_index(genome_name):
    """
    Fixture to load a valid GenomeIndex object for testing
    """
    genome_index_dirname = "data/lutions/indexes"
    genome_index = GenomeIndex.read(genome_index_dirname, genome_name)
    return genome_index


def get_valid_test_methylome(genome_name, tissue_name):
    """
    Fixture to load a valid Methylome object for testing
    """
    methylome_directory = "data/lutions/methylomes"
    methylome_name = f"{genome_name}_{tissue_name}"
    meth = Methylome.read(methylome_directory, methylome_name)
    return meth


def get_valid_test_genomic_intervals(genome_name, tissue_name):
    """
    Fixture to load a valid list of GenomicInterval objects for testing
    """
    intervals_directory = "data/lutions/raw"
    intervals_filename = os.path.join(
        intervals_directory,
        f"{genome_name}_{tissue_name}_hmr.bed",
    )
    genome_index = get_valid_test_genome_index(genome_name)
    intervals = GenomicInterval.read(genome_index, intervals_filename)
    return intervals


def get_valid_test_query(genome_name, tissue_name):
    """
    Fixture to make a valid query for given GenomeIndex and a
    corresponding list of GenomicInterval objects
    """
    intervals = get_valid_test_genomic_intervals(genome_name, tissue_name)
    genome_index = get_valid_test_genome_index(genome_name)
    query = genome_index.make_query(intervals)
    assert query is not None, f"Failed to make query from fixture"
    return query


def create_temp_directory():
    """Create a unique temporary directory in /tmp"""
    temp_dir = tempfile.mkdtemp(dir="/tmp", prefix="test_")
    return temp_dir


def test_methylome_init():
    """Test the default constructor"""
    obj = Methylome()
    assert obj is not None


def test_methylome_read():
    """Test the read static method"""
    directory = "data/lutions/methylomes"
    methylome_name = "eFlareon_brain"
    meth = Methylome.read(directory, methylome_name)
    assert meth is not None


def test_methylome_is_consistent():
    """Test the is_consistent method (empty Methylome)"""
    meth = Methylome()
    assert isinstance(meth.is_consistent(), bool)


def test_methylome_is_consistent():
    """Test the is_consistent method (non-empty Methylome)"""
    directory = "data/lutions/methylomes"
    methylome_name = "eFlareon_brain"
    meth = Methylome.read(directory, methylome_name)
    assert meth.is_consistent()


def test_methylome_is_consistent_with_other():
    """Test the is_consistent method (with another methylome object)"""
    obj1 = Methylome()
    obj2 = Methylome()
    assert isinstance(obj1.is_consistent(obj2), bool)


def test_methylome_write():
    """Test the write method"""
    genome_name = "eVaporeon"
    tissue_name = "brain"
    methylome_name = f"{genome_name}_{tissue_name}"
    meth = get_valid_test_methylome(genome_name, tissue_name)
    output_directory = create_temp_directory()
    meth.write(output_directory, methylome_name)
    if os.path.isdir(output_directory):
        shutil.rmtree(output_directory)


def test_methylome_init_metadata_inconsistent():
    """
    Test init_metadata method when Methylome and GenomeIndex are not
    consistent
    """
    genome_name1 = "eVaporeon"
    genome_name2 = "eJolteon"
    tissue_name = "ear"
    index = get_valid_test_genome_index(genome_name1)
    meth = get_valid_test_methylome(genome_name2, tissue_name)
    with pytest.raises(RuntimeError, match="invalid methylome data"):
        meth.init_metadata(index)


def test_methylome_init_metadata_consistent():
    """
    Test init_metadata method when Methylome and GenomeIndex are
    consistent
    """
    genome_name = "eFlareon"
    tissue_name = "tail"
    index = get_valid_test_genome_index(genome_name)
    meth = get_valid_test_methylome(genome_name, tissue_name)
    try:
        meth.init_metadata(index)
    except RuntimeError as run_err:
        pytest.fail(f"Unexpected exception raised: {run_err}")


def test_methylome_update_metadata_empty():
    """Test update_metadata method for an empty Methylome"""
    meth = Methylome()
    try:
        meth.update_metadata()
    except RuntimeError as run_err:
        pytest.fail(f"Unexpected exception raised: {run_err}")


def test_methylome_update_metadata():
    """Test update_metadata method"""
    genome_name = "eVaporeon"
    tissue_name = "brain"
    meth = get_valid_test_methylome("eVaporeon", "brain")
    try:
        meth.update_metadata()
    except RuntimeError as run_err:
        pytest.fail(f"Unexpected exception raised: {run_err}")


def test_methylome_add_empty():
    """Test add method for two empty methylomes"""
    meth1 = get_valid_test_methylome("eJolteon", "brain")
    meth2 = get_valid_test_methylome("eJolteon", "ear")
    meth1.add(meth2)
    assert meth1 is not None


def test_methylome_repr():
    """Test __repr__ method"""
    meth = Methylome()
    repr_result = repr(meth)
    assert isinstance(repr_result, str)
    assert "version" in repr_result


def test_methylome_get_levels_with_query_container():
    """Test get_levels method (with query_container argument)"""
    meth = get_valid_test_methylome("eFlareon", "brain")
    query = get_valid_test_query("eFlareon", "brain")
    levels = meth.get_levels(query)
    assert isinstance(levels, MLevels)


def test_methylome_get_levels_covered_with_query_container():
    """Test get_levels_covered method (with query_container argument)"""
    meth = get_valid_test_methylome("eJolteon", "ear")
    query = get_valid_test_query("eJolteon", "ear")
    levels = meth.get_levels_covered(query)
    assert isinstance(levels, MLevelsCovered)


def test_methylome_get_levels_with_bin_size_and_genome_index():
    """Test get_levels method (with bin_size and genome_index argument)"""
    genome_name = "eJolteon"
    tissue_name = "ear"
    meth = get_valid_test_methylome(genome_name, tissue_name)
    genome_index = get_valid_test_genome_index(genome_name)
    bin_size = 100
    levels = meth.get_levels(bin_size, genome_index)
    assert isinstance(levels, MLevels)


def test_methylome_get_levels_covered_with_bin_size_and_genome_index():
    """Test get_levels_covered method (with bin_size and genome_index argument)"""
    genome_name = "eJolteon"
    tissue_name = "ear"
    meth = get_valid_test_methylome(genome_name, tissue_name)
    genome_index = get_valid_test_genome_index(genome_name)
    bin_size = 100
    levels = meth.get_levels_covered(bin_size, genome_index)
    assert isinstance(levels, MLevelsCovered)


def test_methylome_global_levels():
    """Test global_levels method"""
    meth = Methylome()
    result = meth.global_levels()
    assert isinstance(result, tuple), "result should be a tuple"
    assert len(result) == 2, "result should have 2 elements"
    assert result[0] == 0


def test_methylome_global_levels_covered():
    """Test global_levels_covered method"""
    meth = Methylome()
    result = meth.global_levels_covered()
    assert isinstance(result, tuple), "result should be a tuple"
    assert len(result) == 3, "result should have 3 elements"
    assert result[2] == 0
