#!/usr/bin/env python3
import argparse
import enum
import numpy as np
from PIL import Image, ImageFilter
from numpy.typing import NDArray

TILE_WIDTH = 8  # Width of each tile (in pixels)
TILE_HEIGHT = 10  # Height of each tile (in pixels)
QUANTIZATION_LEVELS = np.array([0, 40, 80, 120, 160, 200, 230, 255])


class AllowMosaic(enum.Enum):
    No = "no"
    JointOnly = "joint-only"
    Yes = "yes"

    def __str__(self) -> str:
        return self.value


class Tile:
    def __init__(self, pixels: NDArray[np.bool_]):
        assert pixels.shape == (TILE_HEIGHT, TILE_WIDTH)
        assert pixels.dtype == np.bool_
        self.pixels = pixels


class MosaicTile(Tile):
    # The correspondence between codes and pixels is hardcoded in the video
    # chip. Here we reproduce it so we can compute the distance metrics.
    def __init__(self, code: int):
        assert 0 <= code < 0x80

        # Generate 3x2 matrix of blocks.
        blocks = np.array(
            [
                [bool(code & 0x01), bool(code & 0x02)],
                [bool(code & 0x04), bool(code & 0x08)],
                [bool(code & 0x10), bool(code & 0x20)],
            ]
        )

        # Expand into a 10x8 matrix of pixels.
        pixels = blocks[[0, 0, 0, 1, 1, 1, 1, 2, 2, 2]].repeat(4, 1)

        # If disjoint, hide the pixels on the grid.
        if not (code & 0x40):
            pixels[:, [0, 4]] = False
            pixels[[2, 6, 9], :] = False

        super().__init__(pixels)
        self.code = code


class CustomTile(Tile):
    def __init__(self, pixels: NDArray[np.bool_]):
        super().__init__(pixels)
        self.code = None


class LinkedTile:
    def __init__(self, linked_tile_index: int):
        self.linked_tile_index = linked_tile_index


# List of all the glyphs in the custom font and, optionally, in the mosaic
# character set.
class TileSet:
    def __init__(self, allow_mosaic: AllowMosaic):
        match allow_mosaic:
            case AllowMosaic.No:
                self.tiles = []
            case AllowMosaic.JointOnly:
                self.tiles = [MosaicTile(0x40 + c) for c in range(0x40)]
            case AllowMosaic.Yes:
                self.tiles = [MosaicTile(c) for c in range(0x80)]

    # Inserts a new custom tile and return its index.
    def intern_tile(self, pixels: NDArray[np.bool_]) -> int:
        # We could be smarter and only create a new tile if there is no
        # pre-existing perfect match, but it is OK to always create a new one,
        # as matching tiles will be pruned by merge_similar_tiles() anyway.
        index = len(self.tiles)
        self.tiles.append(CustomTile(pixels))
        return index

    # Merges similar tiles until the number of custom tiles in the set becomes
    # less then or equal to the given target.
    def merge_similar_tiles(
        self,
        max_custom_tiles: int,
        distribution: NDArray[np.uint],
        pyramid_min_height: int,
        pyramid_max_height: int,
        pyramid_blur_radius: float,
    ):
        # Turn the distribution of values into a column vector, so that it can
        # later be multiplied with the loss matrix to obtain the loss over the
        # overall image. In order to avoid generating NaNs when multiplying an
        # Infinity loss value by 0 occurrences, we set zeros to be Infinity too.
        distribution = distribution.astype(np.float_)[:, np.newaxis]
        distribution[distribution == 0] = np.Infinity

        # Generate the Gaussian pyramid of each tile.
        pyramids = np.array(
            [
                pyramid(
                    tile.pixels,
                    pyramid_min_height,
                    pyramid_max_height,
                    pyramid_blur_radius,
                )
                for tile in self.tiles
            ]
        ).reshape(len(self.tiles), -1)

        # Compute quality losses[i, j] due to replacing tile i with tile j.
        losses = np.full((len(self.tiles), len(self.tiles)), np.Infinity)
        for i in range(len(self.tiles)):
            if not isinstance(self.tiles[i], CustomTile):
                continue  # only custom tiles can be replaced.

            losses[i] = np.sum((pyramids - pyramids[i]) ** 2, axis=1)
            losses[i, i] = np.Infinity  # cannot replace tile with itself!

        while True:
            # Count the remaining custom tiles.
            num_custom_tiles = 0
            for i in range(len(self.tiles)):
                if isinstance(self.tiles[i], CustomTile):
                    num_custom_tiles += 1
            if num_custom_tiles <= max_custom_tiles:
                break

            # Do the replacement.
            i, j = np.unravel_index(
                np.argmin(losses * distribution),
                losses.shape,
            )
            self.tiles[i] = LinkedTile(j)

            # Update tables.
            losses[i, :] = np.Infinity
            losses[:, i] = np.Infinity
            distribution[j] += distribution[i]
            distribution[i] = np.Infinity  # conceptually 0, but see above.

    # Assigns the "code" field of each custom tile to sequential values,
    # starting from 0.
    def assign_codes(self):
        next_code = 0
        for tile in self.tiles:
            if isinstance(tile, CustomTile):
                tile.code = next_code
                next_code += 1

    # Retrieves the tile at the given index, following links.
    def __getitem__(self, tile_index: int) -> Tile:
        tile = self.tiles[tile_index]
        while isinstance(tile, LinkedTile):
            tile = self.tiles[tile.linked_tile_index]
        return tile


# Given a monochromatic image, returns its Gaussian pyramid (i.e. the
# concatenation of all the images obtained by repeatedly blurring and
# downsampling).
def pyramid(
    elem: NDArray[np.bool_],
    min_height: int,
    max_height: int,
    blur_radius: float,
) -> NDArray[np.float_]:
    flt = ImageFilter.GaussianBlur(blur_radius)
    image = Image.fromarray(elem).convert("L")
    results = [image.getdata()]
    while image.size != (1, 1):
        image = image.filter(flt).resize(
            (
                max(image.size[0] // 2, 1),
                max(image.size[1] // 2, 1),
            ),
            resample=Image.Resampling.BILINEAR,
        )
        results.append(image.getdata())
    return np.concatenate(results[min_height : max_height + 1]) / 255


# Loads an arbitrary input image and turns it into a grayscale image with the
# given size.
def load_image(
    path: str,
    forced_width: int,
    forced_height: int,
) -> NDArray[np.bool_]:
    grayscale_image = Image.open(path).convert("L")
    resized_image = grayscale_image.resize(
        size=(forced_width, forced_height),
        resample=Image.Resampling.NEAREST,
    )
    return np.array(resized_image, dtype=np.uint8)


# Dither the given grayscale image. The output is another grayscale image whose
# values are either 0 or 255.
def dither_image(image: NDArray[np.uint8]) -> NDArray[np.uint8]:
    dithered = Image.fromarray(image).convert("1")
    return np.array(dithered, dtype=np.uint8) * 255


# Given a grayscale image, quantize its colors to the Minitel's palette. Then,
# for each tile, get the minimum and the maximum values and binarize the pixels.
def normalize_image(
    orig: NDArray[np.uint8],
) -> tuple[NDArray[np.bool_], NDArray[np.uint8], NDArray[np.uint8]]:
    # Reduce the number of grayscale levels to 8.
    quantized = np.digitize(orig, QUANTIZATION_LEVELS, right=True)

    tiles = (
        quantized.reshape(
            orig.shape[0] // TILE_HEIGHT,
            TILE_HEIGHT,
            orig.shape[1] // TILE_WIDTH,
            TILE_WIDTH,
        )
        .transpose(0, 2, 1, 3)
        .reshape(
            orig.shape[0] // TILE_HEIGHT,
            orig.shape[1] // TILE_WIDTH,
            TILE_HEIGHT * TILE_WIDTH,
        )
    )

    # Find the minimum and maximum values in each tile.
    levels0 = tiles.min(-1)
    levels1 = tiles.max(-1)

    # Binarize the image using (level0+level1)/2 as the threshold.
    threshold_repeated = np.repeat(
        levels0 + levels1,
        repeats=TILE_HEIGHT * TILE_WIDTH,
    ).reshape(tiles.shape)
    tiles_binarized = 2 * tiles >= threshold_repeated

    # In order to avoid having tiles that are simply negatives of other tiles,
    # flip all the bits if the median value is True (any other deterministic
    # property would do too).
    for r in range(tiles.shape[0]):
        for c in range(tiles.shape[1]):
            if np.median(tiles_binarized[r, c]):
                # Flip all the bits.
                tiles_binarized[r, c] = np.invert(tiles_binarized[r, c])

                # Swap the levels too, so that the image can still be
                # reconstructed.
                levels0[r, c], levels1[r, c] = levels1[r, c], levels0[r, c]

    binarized_image = (
        tiles_binarized.reshape(
            tiles.shape[0],
            tiles.shape[1],
            TILE_HEIGHT,
            TILE_WIDTH,
        )
        .transpose(0, 2, 1, 3)
        .reshape(orig.shape)
    )

    return binarized_image, levels0, levels1


# Given a monochromatic image, returns a matrix of tile indices and the
# corresponding dictionary.
def subdivide_into_tiles(
    src: NDArray[np.bool_],
    allow_mosaic: AllowMosaic,
) -> tuple[NDArray[np.uint], TileSet]:
    src_tiled = (
        src.reshape(
            src.shape[0] // TILE_HEIGHT,
            TILE_HEIGHT,
            src.shape[1] // TILE_WIDTH,
            TILE_WIDTH,
        )
        .transpose(0, 2, 1, 3)
        .reshape(
            src.shape[0] // TILE_HEIGHT,
            src.shape[1] // TILE_WIDTH,
            TILE_HEIGHT * TILE_WIDTH,
        )
    )
    tile_set = TileSet(allow_mosaic)
    tile_indices = np.apply_along_axis(
        lambda src_tile: tile_set.intern_tile(
            src_tile.reshape(TILE_HEIGHT, TILE_WIDTH)
        ),
        axis=2,
        arr=src_tiled,
    )
    return tile_indices, tile_set


# Simulates the Minitel's rendering of the given tiles.
def generate_debug_image(
    tile_indices: NDArray[np.uint8],
    tile_set: TileSet,
    levels0: NDArray[np.uint8],
    levels1: NDArray[np.uint8],
) -> NDArray[np.uint8]:
    values0 = QUANTIZATION_LEVELS[levels0]
    values1 = QUANTIZATION_LEVELS[levels1]
    result = np.zeros(
        (
            tile_indices.shape[0],
            tile_indices.shape[1],
            TILE_HEIGHT,
            TILE_WIDTH,
        ),
        dtype=np.uint8,
    )
    for r in range(tile_indices.shape[0]):
        for c in range(tile_indices.shape[1]):
            result[r, c] = np.where(
                tile_set[tile_indices[r, c]].pixels,
                values1[r, c],
                values0[r, c],
            )

    return result.transpose(0, 2, 1, 3).reshape(
        tile_indices.shape[0] * TILE_HEIGHT,
        tile_indices.shape[1] * TILE_WIDTH,
    )


def generate_source_code(
    identifier: str,
    tile_indices: NDArray[np.uint8],
    tile_set: TileSet,
    levels0: NDArray[np.uint8],
    levels1: NDArray[np.uint8],
) -> str:
    # Mapping from linear luminance to gray index in the Minitel's palette.
    GRAYSCALE = [0, 4, 1, 5, 2, 6, 3, 7]

    outlines = []
    outlines.append(f"FONT_BEGIN({identifier})")
    for tile in tile_set.tiles:
        if isinstance(tile, CustomTile):
            outlines.append(f"\tFONT_GLYPH( // {tile.code}")
            for scanline in range(10):
                bits = "".join("1" if v else "0" for v in tile.pixels[scanline])
                maybe_comma = "," if scanline != 9 else ""
                outlines.append(f"\t\t0b{bits[::-1]}{maybe_comma}")
            outlines.append(f"\t),")
    outlines.append(f"FONT_END")

    outlines.append(f"SCREEN_BEGIN({identifier})")
    for r in range(tile_indices.shape[0]):
        outlines.append(f"\t// row {r}")
        for c in range(tile_indices.shape[1]):
            tile = tile_set[tile_indices[r, c]]
            if isinstance(tile, MosaicTile):
                block_type = "SCREEN_MOSAIC"
            elif isinstance(tile, CustomTile):
                block_type = "SCREEN_CUSTOM"
            else:
                raise RuntimeError  # this should never happen
            code = tile.code
            level0 = GRAYSCALE[levels0[r, c]]
            level1 = GRAYSCALE[levels1[r, c]]
            outlines.append(f"\t{block_type}({code},{level0},{level1}), // {c}")
    outlines.append(f"SCREEN_END")

    return "\n".join(outlines)


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Converts arbitrary images into a screen made of tiles of mosaic "
            "and custom-font characters."
        )
    )

    parser.add_argument(
        "input_image",
        metavar="INPUT_PATH",
        help="input image",
    )
    parser.add_argument(
        "output_source_code_file",
        metavar="OUTPUT_PATH",
        help="generated source code output file",
    )
    parser.add_argument(
        "--dither",
        action="store_true",
        help="dither before processing",
    )
    parser.add_argument(
        "--source-code-identifier",
        default="image",
        help="identifier that will be used in the generated source code",
    )

    debug = parser.add_argument_group("debug options")
    debug.add_argument(
        "--debug-input-image",
        metavar="PATH",
        help="write the preprocessed input image",
    )
    debug.add_argument(
        "--debug-output-image",
        metavar="PATH",
        help="write preview of the output image",
    )

    tuning = parser.add_argument_group("tuning parameters")
    tuning.add_argument(
        "--display-columns",
        type=int,
        default=40,
        help="width of the displayed image (unit: number of tiles)",
    )
    tuning.add_argument(
        "--display-rows",
        type=int,
        default=25,
        help="height of the displayed image (unit: number of tiles)",
    )
    tuning.add_argument(
        "--max-custom-tiles",
        type=int,
        default=300,
        help="maximum allowed number of custom tiles",
    )
    tuning.add_argument(
        "--blur-radius",
        type=float,
        default=2,
        help="radius of the Gaussian blur for the distance metric",
    )
    tuning.add_argument(
        "--min-pyramid-height",
        type=int,
        help="minimum Gaussian pyramid height (lower levels will be ignored)",
    )
    tuning.add_argument(
        "--max-pyramid-height",
        type=int,
        help="maximum Gaussian pyramid height (higher levels will be ignored)",
    )
    tuning.add_argument(
        "--allow-mosaic",
        choices=list(AllowMosaic),
        default=AllowMosaic.JointOnly,
        type=AllowMosaic,
        help="allowed mosaic characters",
    )

    args = parser.parse_args()

    # Assign presets, unless they have been overridden.
    if args.min_pyramid_height is None:
        args.min_pyramid_height = 1 if args.dither else 0
    if args.max_pyramid_height is None:
        args.max_pyramid_height = 1 if args.dither else 3

    # Load the input image and resize it to match the display size.
    input_image = load_image(
        path=args.input_image,
        forced_width=args.display_columns * TILE_WIDTH,
        forced_height=args.display_rows * TILE_HEIGHT,
    )

    # Dither the image, if requested.
    if args.dither:
        input_image = dither_image(input_image)

    # Write the preprocessed input image (debug only).
    if args.debug_input_image:
        Image.fromarray(input_image).save(args.debug_input_image)

    # Assign background and foreground colors to each tile and constrain each
    # pixel to be one of those.
    binarized, levels0, levels1 = normalize_image(input_image)

    # Generate tiles, constraining the number of custom tiles.
    tile_indices, tile_set = subdivide_into_tiles(binarized, args.allow_mosaic)
    tile_set.merge_similar_tiles(
        args.max_custom_tiles,
        np.bincount(tile_indices.flatten()),
        args.min_pyramid_height,
        args.max_pyramid_height,
        args.blur_radius,
    )

    # Write the resulting output image (debug only).
    debug_image = generate_debug_image(tile_indices, tile_set, levels0, levels1)
    if args.debug_output_image:
        Image.fromarray(debug_image).save(args.debug_output_image)

    # Write the resulting tiles as C source code.
    tile_set.assign_codes()
    source_code = generate_source_code(
        args.source_code_identifier, tile_indices, tile_set, levels0, levels1
    )
    with open(args.output_source_code_file, "wt") as fp:
        fp.write(source_code)


if __name__ == "__main__":
    main()
