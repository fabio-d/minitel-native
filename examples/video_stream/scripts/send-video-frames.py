#!/usr/bin/env python3
import argparse
import numpy as np
import serial
from PIL import Image, ImageSequence, ImageEnhance

TILES_X = 40
TILES_Y = 25
TILE_W = 2
TILE_H = 3


# Turns an arbitrary image into a sequence of mosaic characters (preceeded by
# 0xFF, which delimits the beginning of a new screen).
def prepare_image(image: Image.Image, contrast: float) -> bytes:
    image = ImageEnhance.Contrast(image.convert("L")).enhance(contrast)

    dithered = image.resize((TILES_X * TILE_W, TILES_Y * TILE_H)).convert("1")
    pixels = np.array(dithered)
    tiles = (
        pixels.reshape(TILES_Y, TILE_H, TILES_X, TILE_W)
        .transpose(0, 2, 1, 3)
        .reshape(TILES_Y, TILES_X, -1)
    )

    packet = [0xFF]  # marks the beginning of a new screen
    for r in range(TILES_Y):
        for c in range(TILES_X):
            mosaic_code = 0
            for i, v in enumerate(tiles[r, c]):
                if v:
                    mosaic_code |= 1 << i
            packet.append(mosaic_code | (1 << 6))

    return packet


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image_path", help="Animated image to play")
    parser.add_argument("--serial-port", help="Serial port name")
    parser.add_argument("--baud-rate", help="Serial baud rate")

    parser.add_argument(
        "--contrast",
        type=float,
        default=5.0,
        help="Adjust contrast",
    )

    args = parser.parse_args()

    packets = []
    with Image.open(args.image_path) as im:
        for frame in ImageSequence.Iterator(im):
            packets.append(prepare_image(frame, args.contrast))

    ser = serial.Serial(args.serial_port, baudrate=args.baud_rate)
    while True:
        for p in packets[1:]:
            ser.write(p)
            ser.flush()


if __name__ == "__main__":
    main()
