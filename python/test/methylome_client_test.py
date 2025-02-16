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

from transferase import MethylomeClient

def create_temp_directory():
    """Create a unique temporary directory in /tmp"""
    temp_dir = tempfile.mkdtemp(dir="/tmp", prefix="test_")
    return temp_dir

def test_methylome_client_factory():
    """Get a MethylomeClient"""
    config_dir = "data/lutions"
    obj = MethylomeClient(config_dir)
    assert hasattr(obj, "config"), "Object is missing 'config'"

def test_reset_to_default_config():
    """Reset the configuration to the default"""
    config_dir = "data/lutions"
    config_dir_tmp = create_temp_directory()
    obj = MethylomeClient(config_dir)
    obj_tmp = obj
    assert obj_tmp == obj
    if os.path.isdir(config_dir_tmp):
        shutil.rmtree(config_dir_tmp)
