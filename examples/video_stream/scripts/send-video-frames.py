#!/usr/bin/env python3
import argparse
import numpy as np
import serial
import sys
import time
from PIL import Image, ImageSequence, ImageEnhance

TILES_X = 40
TILES_Y = 25
TILE_W = 2
TILE_H = 3

# Parameters for serial.Serial's constructor:
#   (number of configured stop bits) -> (parity, stopbits)
STOP_BITS_TO_ARGS = {
    1: (serial.PARITY_NONE, serial.STOPBITS_ONE),
    2: (serial.PARITY_NONE, serial.STOPBITS_TWO),
    3: (serial.PARITY_MARK, serial.STOPBITS_TWO),  # add 3rd stop bits as parity
}


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
    parser.add_argument(
        "image_path",
        help="Animated image to play",
    )

    parser.add_argument(
        "--serial-port",
        required=True,
        help="Serial port name",
    )

    parser.add_argument(
        "--baud-rate",
        type=int,
        required=True,
        help="Serial baud rate",
    )

    parser.add_argument(
        "--stop-bits",
        type=int,
        default=1,
        choices=[1, 2, 3],
        help="Number of stop bits",
    )

    parser.add_argument(
        "--contrast",
        type=float,
        default=5.0,
        help="Adjust contrast",
    )

    args = parser.parse_args()

    # Preload and convert all the images in the on-wire format.
    packets = []
    with Image.open(args.image_path) as im:
        for frame in ImageSequence.Iterator(im):
            packets.append(prepare_image(frame, args.contrast))

    # Open the serial port and dump its full configuration.
    parity_arg, stopbits_arg = STOP_BITS_TO_ARGS[args.stop_bits]
    ser = serial.Serial(
        args.serial_port,
        baudrate=args.baud_rate,
        parity=parity_arg,
        stopbits=stopbits_arg,
    )
    print(f"Serial port configuration:", file=sys.stderr)
    for key, value in ser.get_settings().items():
        print(f" {key} = {value}", file=sys.stderr)

    # Send all the images in a loop.
    start = time.monotonic()
    count = 0
    try:
        while True:
            for p in packets[1:]:
                ser.write(p)
                ser.flush()
                count += 1
    except KeyboardInterrupt:
        stop = time.monotonic()
        duration = stop - start
        print(f"Sent {count} frames in {duration:.2f} seconds", file=sys.stderr)
        print(f"Average fps: {count / duration:.2f}", file=sys.stderr)


if __name__ == "__main__":
    main()
