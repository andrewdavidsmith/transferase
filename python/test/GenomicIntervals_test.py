# MIT License
#
# Copyright (c) 2025 Andrew D Smith
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


from transferase import GenomicInterval
from transferase import GenomeIndex


def create_temp_directory():
    """
    Create a unique temporary directory in /tmp
    """
    temp_dir = tempfile.mkdtemp(dir="/tmp", prefix="test_")
    return temp_dir


def test_GenomicInterval_init():
    """
    Test the constructor for GenomicInterval
    """
    obj = GenomicInterval()
    assert obj is not None
    assert obj.ch_id == -1
    assert obj.start == 0
    assert obj.stop == 0


def test_GenomicInterval_repr():
    """
    Test the __repr__ function for GenomicInterval
    """
    obj = GenomicInterval()
    assert repr(obj) == repr((-1, 0, 0))


def test_GenomicInterval_to_string(pytestconfig):
    """
    Test the to_string function for GenomicInterval
    """
    rootdir = pytestconfig.rootdir
    index_directory = os.path.join(rootdir, "data/lutions/indexes")
    index = GenomeIndex.read(index_directory, "eFlareon")
    assert index != None
    obj = GenomicInterval()
    obj.ch_id, obj.start, obj.stop = (0, 100, 200)
    assert obj.to_string(index) == repr(("chr1", 100, 200))


def test_GenomicInterval_read(pytestconfig):
    """
    Test the 'read' function for GenomicInterval
    """
    rootdir = pytestconfig.rootdir
    index_directory = os.path.join(rootdir, "data/lutions/indexes")
    index = GenomeIndex.read(index_directory, "eFlareon")
    assert index != None
    intervals_directory = os.path.join(rootdir, "data/lutions/raw")
    intervals_file = os.path.join(intervals_directory, "eFlareon_brain_hmr.bed")
    obj = GenomicInterval.read(index, intervals_file)
    assert obj != None
    assert len(obj) == 10


def test_GenomicInterval_are_valid(pytestconfig):
    """
    Test the 'are_valid' static function for GenomicInterval
    """
    rootdir = pytestconfig.rootdir
    index_directory = os.path.join(rootdir, "data/lutions/indexes")
    index = GenomeIndex.read(index_directory, "eFlareon")
    assert index != None
    intervals_directory = os.path.join(rootdir, "data/lutions/raw")
    intervals_file = os.path.join(intervals_directory, "eFlareon_brain_hmr.bed")
    obj = GenomicInterval.read(index, intervals_file)
    assert GenomicInterval.are_valid(obj)
