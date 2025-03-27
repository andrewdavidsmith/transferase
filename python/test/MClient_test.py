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

from transferase import MClient


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


def test_MClient_constructor(pytestconfig):
    """Get a MClient"""
    config_dir = get_config_dir(pytestconfig)
    obj = MClient(config_dir)
    assert hasattr(obj, "config"), "Object is missing 'config'"


def test_config_makes_sense(pytestconfig):
    """Check that config object isn't broken"""
    config_dir = get_config_dir(pytestconfig)
    obj = MClient(config_dir)
    config = obj.config
    assert config.hostname != None
    assert config.port != None


def test_configured_genomes(pytestconfig):
    """Test the function to list configured genomes"""
    obj = MClient(get_config_dir(pytestconfig))
    assert len(obj.configured_genomes()) == 3
