#!/usr/bin/env python3
import argparse
from PIL import Image
from typing import List, Tuple


# Reads a monochomatic image and splits it into 8x10 tiles.
def read_tiles(path: str) -> List[Image.Image]:
    img = Image.open(path).convert("1")
    assert img.width % 8 == 0, "width must be a multiple of 8"
    assert img.height % 10 == 0, "height must be a multiple of 10"

    results = []
    for y in range(0, img.height, 10):
        for x in range(0, img.width, 8):
            results.append(img.crop((x, y, x + 8, y + 10)))

    return results


# Exports a 8x10 tile as a C array with the given name.
def generate_c_array(name: str, image: Image.Image) -> List[str]:
    output_lines = []
    output_lines.append(f"static __code const uint8_t {name}[] = {{")
    for y in range(10):
        scanline = 0
        for x in range(8):
            scanline |= (1 << x) if image.getpixel((x, y)) else 0
        output_lines.append("0b{:08b},".format(scanline))
    output_lines.append(f"}};")
    return output_lines


def tile_names_and_source_path(text: str) -> Tuple[List[str], str]:
    tile_names, sep, source_path = text.rpartition("=")
    if sep != "=":
        raise ValueError
    return tile_names.split(":"), source_path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        help="output C file",
        required=True,
    )

    parser.add_argument(
        "tiles",
        metavar="TILE_NAME[:TILE_NAME_2...]=IMAGE_PATH",
        nargs="+",
        help="output variable names and corresponding input image files",
        type=tile_names_and_source_path,
    )

    args = parser.parse_args()

    output_lines = ["#include <stdint.h>"]
    for tile_names, source_path in args.tiles:
        tile_images = read_tiles(source_path)
        for name, image in zip(tile_names, tile_images, strict=True):
            output_lines += generate_c_array(name, image)

    with open(args.output, "wt") as fp:
        fp.write("\n".join(output_lines))


if __name__ == "__main__":
    main()
