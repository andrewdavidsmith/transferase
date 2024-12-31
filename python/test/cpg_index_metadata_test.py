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
import transferase
from transferase import cpg_index_metadata
from transferase.cpg_index_metadata import CpgIndexMetadata

class TestCpgIndexMetadata(unittest.TestCase):

    def test_read_from_json(self):
        """
        Test the read function from a JSON file
        """
        ec = cpg_index_metadata.ErrorCode()
        metadata = CpgIndexMetadata.read("data/pAntiquusx.cpg_idx.json", ec)
        self.assertIsNotNone(metadata)
        self.assertIsInstance(metadata, CpgIndexMetadata)

    def test_read_from_directory(self):
        """
        Test the read function from directory and genome name
        """
        ec = cpg_index_metadata.ErrorCode()
        metadata = CpgIndexMetadata.read("data", "tProrsus1", ec)
        self.assertIsNotNone(metadata)
        self.assertIsInstance(metadata, CpgIndexMetadata)

    def test_tostring(self):
        """
        Test the tostring method
        """
        metadata = CpgIndexMetadata()
        result = metadata.tostring()
        print(result)
        self.assertIsInstance(result, str)
        self.assertIn("version", result)

if __name__ == "__main__":
    unittest.main()
