#!/usr/bin/env python3

import sys

prev_was_preinstall = False
for line in open(sys.argv[1]):
    if line.startswith("preinstall:"):
        print("preinstall:")
        prev_was_preinstall = True
    elif prev_was_preinstall:
        print("#")
        prev_was_preinstall = False
    else:
        print(line, end="")
