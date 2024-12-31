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

import unittest
from transferase.query import Query

class TestQuery(unittest.TestCase):

    def setUp(self):
        """Set up initial data for tests"""
        # Initialize a query object with some elements
        self.query1 = Query()
        self.query2 = Query()

    def test_init_empty(self):
        """Test default constructor"""
        query = Query()
        self.assertEqual(len(query), 0)

    def test_repr(self):
        """Test the __repr__ method"""
        query = Query()
        repr_str = repr(query)
        self.assertIn("<Query size=0>", repr_str)

    def test_eq(self):
        """Test the equality operator"""
        self.assertEqual(self.query1, self.query2)

if __name__ == "__main__":
    unittest.main()
