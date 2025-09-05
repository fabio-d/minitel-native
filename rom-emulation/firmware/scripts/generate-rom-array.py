#!/usr/bin/env python3
import sys
from pathlib import Path

# Write the contents of a file as a C array.

input_path = Path(sys.argv[1])
output_path = Path(sys.argv[2])
variable_name = sys.argv[3]

data = input_path.read_bytes()
if len(data) > 0x10000:
    exit("ROM cannot be larger than 64 KiB")

with output_path.open("wt") as fp:
    print("#include <stdint.h>", file=fp)
    print("namespace {", file=fp)
    print("const uint8_t %s[] = {" % variable_name, file=fp)
    for i, c in enumerate(data):
        print("    0x%02x,  // @ 0x%04x" % (c, i), file=fp)
    print("};", file=fp)
    print("}", file=fp)
