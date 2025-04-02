#!/usr/bin/env python3

# Copyright 2025, Andrew D Smith
# SPDX-License-Identifier: MIT

import sys

if len(sys.argv) != 2:
    print("Usage: cpack_no_preinstall.py path/to/makefile", file=sys.stderr)
    sys.exit(1)

found_preinstall = False
prev_was_preinstall = False
for line in open(sys.argv[1]):
    if line.startswith("preinstall:"):
        print("preinstall:")
        prev_was_preinstall = True
        found_preinstall = True
    elif prev_was_preinstall:
        print("#")
        prev_was_preinstall = False
    else:
        print(line, end="")

if not found_preinstall:
    print("Failed to find preinstall target", file=sys.stderr)
    sys.exit(1)
