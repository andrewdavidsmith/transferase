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


from transferase import ClientConfig

def create_temp_directory():
    """Create a unique temporary directory in /tmp"""
    temp_dir = tempfile.mkdtemp(dir="/tmp", prefix="test_")
    return temp_dir


def test_client_config_init():
    """Test the constructor with keywor arguments"""
    obj = ClientConfig.default()
    assert obj is not None


def test_assign_and_write_success():
    """
    Test that data members of the created object can be assigned to
    and that writing succeeds
    """
    config_filename = "transferase_client_config.toml"
    outdir = create_temp_directory()
    obj = ClientConfig.default()
    obj.hostname = "not-not-kernel.org"
    assert obj.write(outdir) is None
    assert os.path.exists(outdir)
    assert os.path.exists(os.path.join(outdir, config_filename))
    if os.path.isdir(outdir):
        shutil.rmtree(outdir)


## ADS: Similar to the case for make_directories_failure in
## test/client_config_test.cpp, the test below can't work on github
## runners, so it's out for now...
# def test_run_fail_bad_config_dir():
#     """
#     Test that the run function will fail if the config dir makes no sense
#     """
#     bad_config_dir_mock = "/dev"
#     obj = ClientConfig()
#     obj.config_dir = bad_config_dir_mock
#     validate_ok = obj.validate()
#     assert validate_ok is True
#     with pytest.raises(RuntimeError) as excinfo:
#         obj.run([], False)
#     assert "error creating directories" in str(excinfo.value)


def test_run_no_genomes_success():
    """
    Test that the run function can succeed, no genomes specified

    ADS: functionality being tested is gone; need to update
    """
    obj = ClientConfig.default()
    assert obj is not None
