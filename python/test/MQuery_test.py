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

from pyxfr import MQuery


@pytest.fixture
def query_containers():
    """Fixture to set up initial data for tests"""
    # Initialize MQuery objects
    query1 = MQuery()
    query2 = MQuery()
    return query1, query2


def test_init_empty(query_containers):
    """Test length zero"""
    query1, _ = query_containers
    assert len(query1) == 0


def test_repr(query_containers):
    """Test the __repr__ method"""
    query1, _ = query_containers
    repr_str = repr(query1)
    assert "<MQuery size=0>" in repr_str


def test_eq(query_containers):
    """Test the equality operator"""
    query1, query2 = query_containers
    assert query1 == query2
