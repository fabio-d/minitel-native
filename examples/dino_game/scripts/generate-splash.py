#!/usr/bin/env python3
import argparse
from PIL import Image
from typing import List, Optional, Tuple


# Reads a monochomatic image and splits it into a grid of 8x10 tiles.
def read_tiles(path: str) -> Tuple[int, int, List[Image.Image]]:
    img = Image.open(path).convert("1")
    assert img.width % 8 == 0, "width must be a multiple of 8"
    assert img.height % 10 == 0, "height must be a multiple of 10"

    results = []
    for y in range(0, img.height, 10):
        for x in range(0, img.width, 8):
            results.append(img.crop((x, y, x + 8, y + 10)))

    return img.width // 8, img.height // 10, results


# Tests if all the pixels of the given image are the same color.
def is_uniform_image(image: Image.Image) -> Optional[int]:
    for y in range(10):
        for x in range(8):
            if image.getpixel((x, y)) != image.getpixel((0, 0)):
                return False
    return True


# Converts the scanlines of a 8x10 tile into a list of C values.
def generate_c_array(image: Image.Image) -> List[str]:
    output_lines = []
    for y in range(10):
        scanline = 0
        for x in range(8):
            scanline |= (1 << x) if image.getpixel((x, y)) else 0
        output_lines.append("0b{:08b},".format(scanline))
    return output_lines


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        help="output C file",
        required=True,
    )

    parser.add_argument(
        "image",
        help="input image file",
    )

    args = parser.parse_args()

    num_cols, num_rows, tile_images = read_tiles(args.image)

    output_lines = ["#include <stdint.h>"]
    output_lines.append(f"#define SPLASH_COLS {num_cols}")
    output_lines.append(f"#define SPLASH_ROWS {num_rows}")
    output_lines.append(f"static __code const uint8_t SPLASH_DATA[] = {{")
    non_uniform_tile_count = 0
    for y in range(num_rows):
        for x in range(num_cols):
            tile = tile_images[y * num_cols + x]
            output_lines.append(f"// Tile x={x} y={y}")
            if is_uniform_image(tile):
                constant_color = 1 if tile.getpixel((0, 0)) else 0
                output_lines.append(f"{constant_color}, // uniform tile")
            else:
                output_lines.append(f"0xFF, // non-uniform tile marker")
                output_lines.extend(generate_c_array(tile))
                non_uniform_tile_count += 1
    output_lines.append(f"}};")

    # print("Non-uniform tiles:", non_uniform_tile_count)
    if non_uniform_tile_count > 100:  # max number of characters in G'0 font
        exit("Too many non-uniform tiles")

    with open(args.output, "wt") as fp:
        fp.write("\n".join(output_lines))


if __name__ == "__main__":
    main()
