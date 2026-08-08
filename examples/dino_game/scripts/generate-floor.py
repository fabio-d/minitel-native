#!/usr/bin/env python3
import argparse
from typing import List, Tuple


# Generate a look-up table with all the possible rotations of 0b111 over 32 bits
def generate_c_array(name: str, x_offset: int) -> List[str]:
    output_lines = []
    output_lines.append(f"static __code const uint8_t {name}[] = {{")
    text = "0" * 27 + "1" * 3  # 32 bits in total
    for i in range(32):
        x = (x_offset + i) % 32
        bits = "0b" + (text[x:] + text[:x])[:8]
        output_lines.append(f"{bits},")
    output_lines.append(f"}};")
    return output_lines


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        help="output C file",
        required=True,
    )

    args = parser.parse_args()

    output_lines = ["#include <stdint.h>"]
    output_lines += generate_c_array("FLOOR_LUT_A", 24)
    output_lines += generate_c_array("FLOOR_LUT_B", 16)
    output_lines += generate_c_array("FLOOR_LUT_C", 8)
    output_lines += generate_c_array("FLOOR_LUT_D", 0)

    with open(args.output, "wt") as fp:
        fp.write("\n".join(output_lines))


if __name__ == "__main__":
    main()
