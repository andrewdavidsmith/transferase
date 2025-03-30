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

from pyxfr import MClientLocal
from pyxfr import MConfig
from pyxfr import GenomicInterval
from pyxfr import MQuery
from pyxfr import GenomeIndex


def create_temp_directory():
    """Create a unique temporary directory in /tmp"""
    temp_dir = tempfile.mkdtemp(dir="/tmp", prefix="test_")
    return temp_dir


def get_config_dir(pytestconfig):
    """
    Get the config_dir using the rootdir from pytestconfig
    """
    lutions_config_dir = "data/lutions"
    return os.path.join(pytestconfig.rootdir, lutions_config_dir)


def test_MClientLocal_constructor(pytestconfig):
    """Get a MClientLocal"""
    obj = MClientLocal(get_config_dir(pytestconfig))
    assert hasattr(obj, "config"), "Object is missing 'config'"


def test_reset_to_default_config(pytestconfig):
    """Reset the configuration to the default"""
    obj = MClientLocal(get_config_dir(pytestconfig))
    obj_tmp = obj
    assert obj_tmp == obj


def test_getters(pytestconfig):
    """Test the getter functions"""
    obj = MClientLocal(get_config_dir(pytestconfig))
    assert os.path.basename(obj.get_index_dir()) == "indexes"
    assert os.path.basename(obj.get_methylome_dir()) == "methylomes"
    assert isinstance(obj.config, MConfig)


def test_configured_genomes(pytestconfig):
    """Test the function to list configured genomes"""
    obj = MClientLocal(get_config_dir(pytestconfig))
    assert len(obj.configured_genomes()) == 3


def test_get_levels(pytestconfig):
    """Test the function to list configured genomes"""
    rootdir = pytestconfig.rootdir
    methylomes = [
        "eFlareon_brain",
        "eFlareon_tail",
    ]
    index_directory = os.path.join(rootdir, "data/lutions/indexes")
    index = GenomeIndex.read(index_directory, "eFlareon")
    assert index != None
    intervals_directory = os.path.join(rootdir, "data/lutions/raw")
    intervals_file = os.path.join(intervals_directory, "eFlareon_brain_hmr.bed")
    intervals = GenomicInterval.read(index, intervals_file)
    obj = MClientLocal(get_config_dir(pytestconfig))
    levels = obj.get_levels(methylomes, intervals)
    assert levels.n_rows == len(intervals)
    assert levels.n_cols == len(methylomes)
    assert levels.at(1, 1) == (56, 7)  # Known
