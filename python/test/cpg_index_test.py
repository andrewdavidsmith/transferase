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
from transferase import CpgIndex
from transferase import ErrorCode
from transferase import GenomicInterval

def create_temp_directory():
    """Create a unique temporary directory in /tmp"""
    temp_dir = tempfile.mkdtemp(dir="/tmp", prefix="my_test_")
    return temp_dir

@pytest.fixture
def cpg_index():
    """Fixture to create a CpgIndex object for testing"""
    return CpgIndex()

def test_data_property(cpg_index):
    """Test the 'data' readonly property"""
    assert hasattr(cpg_index, "data")

def test_meta_property(cpg_index):
    """Test the 'meta' readonly property"""
    assert hasattr(cpg_index, "meta")

def test_is_consistent(cpg_index):
    """Test the 'is_consistent' method"""
    assert isinstance(cpg_index.is_consistent(), bool)

def test_hash(cpg_index):
    """Test the '__hash__' method"""
    assert isinstance(hash(cpg_index), int)

def test_repr(cpg_index):
    """Test the '__repr__' method"""
    repr_str = repr(cpg_index)
    assert isinstance(repr_str, str)
    assert len(repr_str) > 0

def test_read():
    """Test the static 'read' method"""
    dirname = "data/lutions/indexes"
    genome_name = "eVaporeon"
    error = ErrorCode()
    result = CpgIndex.read(dirname, genome_name, error)
    assert not error
    assert result is not None

def test_write(cpg_index):
    """Test the 'write' method"""
    outdir = create_temp_directory()
    name = "test_name"
    status = cpg_index.write(outdir, name)
    assert not status
    if os.path.isdir(outdir):
        shutil.rmtree(outdir)

def test_make_query(cpg_index):
    """Test the 'make_query' method"""
    error = ErrorCode()
    intervals_file = "data/lutions/raw/eVaporeon_ear_hmr.bed"
    intervals = GenomicInterval.read(cpg_index, intervals_file, error)
    result = cpg_index.make_query(intervals)
    assert result is not None  # Modify based on expected output

def test_make_cpg_index():
    """Test the static 'make_cpg_index' method"""
    genome_file = "data/lutions/raw/eJolteon.fa.gz"
    status = ErrorCode()
    result = CpgIndex.make_cpg_index(genome_file, status)
    assert not status
    assert result is not None

def test_files_exist():
    """Test the static 'files_exist' method"""
    directory = "data/lutions/indexes"
    genome_name = "eJolteon"
    result = CpgIndex.files_exist(directory, genome_name)
    assert isinstance(result, bool)
    assert result

def test_parse_genome_name():
    """Test the static 'parse_genome_name' method"""
    filename = "eFlareon.fasta.gz"
    error_code = ErrorCode()
    result = CpgIndex.parse_genome_name(filename, error_code)
    assert isinstance(result, str)
    assert result == "eFlareon"

def test_list_cpg_indexes():
    """Test the static 'list_cpg_indexes' method"""
    directory = "data/lutions/indexes"
    error_code = ErrorCode()
    result = CpgIndex.list_cpg_indexes(directory, error_code)
    assert isinstance(result, list)
    assert len(result) == 3
