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
from transferase import GenomicInterval


def create_temp_directory():
    """Create a unique temporary directory in /tmp"""
    temp_dir = tempfile.mkdtemp(dir="/tmp", prefix="test_")
    return temp_dir


@pytest.fixture
def genome_index():
    """Fixture to create a GenomeIndex object for testing"""
    return GenomeIndex()


def get_rootdir(pytestconfig):
    return pytestconfig.option["rootdir"]


def test_is_consistent(genome_index):
    """Test the 'is_consistent' method"""
    assert isinstance(genome_index.is_consistent(), bool)


def test_hash(genome_index):
    """Test the '__hash__' method"""
    assert isinstance(hash(genome_index), int)


def test_repr(genome_index):
    """Test the '__repr__' method"""
    repr_str = repr(genome_index)
    assert isinstance(repr_str, str)
    assert len(repr_str) > 0


def test_read(pytestconfig):
    """Test the static 'read' method"""
    rootdir = pytestconfig.rootdir
    dirname = os.path.join(rootdir, "data/lutions/indexes")
    genome_name = "eVaporeon"
    try:
        index = GenomeIndex.read(dirname, genome_name)
    except Exception as err:
        print(f"CWD={os.getcwd()}\nROOTDIR={pytest.rootdir}")
        raise err
    assert index is not None


def test_write(genome_index):
    """Test the 'write' method"""
    outdir = create_temp_directory()
    name = "test_name"
    status = genome_index.write(outdir, name)
    assert not status
    if os.path.isdir(outdir):
        shutil.rmtree(outdir)


def test_make_query(genome_index, pytestconfig):
    """Test the 'make_query' method"""
    rootdir = pytestconfig.rootdir
    intervals_file = os.path.join(
        rootdir, "data/lutions/raw/eVaporeon_ear_hmr.bed"
    )
    with pytest.raises(RuntimeError, match="chrom name not found in index"):
        intervals = GenomicInterval.read(genome_index, intervals_file)
        result = genome_index.make_query(intervals)
        assert result is not None  # Modify based on expected output


def test_make_genome_index(pytestconfig):
    """Test the static 'make_genome_index' method"""
    rootdir = pytestconfig.rootdir
    genome_file = os.path.join(rootdir, "data/lutions/raw/eJolteon.fa.gz")
    result = GenomeIndex.make_genome_index(genome_file)
    assert result is not None


def test_files_exist(pytestconfig):
    """Test the static 'files_exist' method"""
    rootdir = pytestconfig.rootdir
    directory = os.path.join(rootdir, "data/lutions/indexes")
    genome_name = "eJolteon"
    result = GenomeIndex.files_exist(directory, genome_name)
    assert isinstance(result, bool)
    assert result


def test_list_genome_indexes(pytestconfig):
    """Test the static 'list_genome_indexes' method"""
    rootdir = pytestconfig.rootdir
    directory = os.path.join(rootdir, "data/lutions/indexes")
    result = GenomeIndex.list_genome_indexes(directory)
    assert isinstance(result, list)
    assert len(result) == 3
