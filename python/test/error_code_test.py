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

from transferase import ErrorCode

@pytest.fixture
def error_code_fixture():
    """Fixture to set up initial data for tests"""
    # Initialize an ErrorCode object
    ec = ErrorCode()
    return ec

def test_error_code_repr(error_code_fixture):
    """
    Test the __repr__ method
    """
    error_code = error_code_fixture
    repr_result = repr(error_code)
    assert isinstance(repr_result, str)

def test_error_code_value(error_code_fixture):
    """
    Test the value() method
    """
    error_code = error_code_fixture
    value = error_code.value()
    assert isinstance(value, int)

def test_error_code_message(error_code_fixture):
    """
    Test the message() method
    """
    error_code = error_code_fixture
    message = error_code.message()
    assert isinstance(message, str)
