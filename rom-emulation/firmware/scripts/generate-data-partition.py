#!/usr/bin/env python3
import argparse
import struct

BLOCK_SIZE = 0x1000
NUM_SUPERBLOCKS = 16
MAX_ROM_SIZE = 64 * 1024


def main():
    parser = argparse.ArgumentParser(
        prog="generate-data-partition",
        description="Generates a prefilled data partition image.",
    )

    for slot_num in range(16):
        parser.add_argument(
            f"--slot{slot_num:X}",
            metavar=("rom.bin", "text"),
            nargs=2,
            help=f"ROM binary file and title for slot {slot_num:X}",
        )

    parser.add_argument(
        "--wl-ssid",
        metavar="NETWORK",
        help="Wireless network name to connect to",
    )

    parser.add_argument(
        "--wl-psk",
        metavar="PASSWORD",
        help="Wireless network password (empty string for open networks)",
    )

    parser.add_argument(
        "--output",
        metavar="PATH",
        help="Output path for the resulting data partition image",
        required=True,
    )

    args = parser.parse_args()

    if args.wl_ssid is not None:
        wl_ssid = args.wl_ssid.encode("utf-8")
        if not (0 < len(wl_ssid) <= 32):
            parser.error("--wl-ssid has an invalid length")
        if args.wl_psk is None:
            parser.error("--wl-ssid requires --wl-psk")
        wl_psk = args.wl_psk.encode("utf-8")
        if len(wl_psk) == 0:
            wl_type = 0  # open network
        else:
            if not (8 <= len(wl_psk) <= 63):
                parser.error("--wl-psk has an invalid length")
            wl_type = 1  # WPA network
    else:
        wl_ssid = b""
        wl_psk = b""
        wl_type = 0xFF  # not configured

    # Load the requested ROM files.
    roms = []
    for slot_num in range(16):
        if slot_value := getattr(args, f"slot{slot_num:X}"):
            path, name = slot_value
            with open(path, "rb") as fp:
                # Read up to MAX_ROM_SIZE+1, so we can detect if the ROM file
                # if larger than MAX_ROM_SIZE.
                contents = fp.read(MAX_ROM_SIZE + 1)
            if len(contents) > MAX_ROM_SIZE:
                exit(f"{path} cannot be larger than {MAX_ROM_SIZE} bytes")
            roms.append((contents, name))
        else:
            roms.append((None, ""))

    # Generate the partition image.
    with open(args.output, "wb") as fp:
        # Write the first superblock.
        fp.write(struct.pack("<I", 0xFFFFFFFE))  # generation counter
        for contents, name in roms:
            fp.write(
                struct.pack(
                    "<I127p",
                    0xFFFFFFFF if contents is None else len(contents),
                    name.encode(),  # TODO: encode nonstandard characters too
                )
            )
        fp.write(struct.pack("<B32sx63sx", wl_type, wl_ssid, wl_psk))

        # Ensure all the remaining superblocks are filled with 0xFF to
        # invalidate them.
        start_pos = BLOCK_SIZE * NUM_SUPERBLOCKS
        fp.write(bytes([0xFF]) * (start_pos - fp.tell()))

        # Store ROM contents at the corresponding locations.
        for contents, name in roms:
            if contents is not None:
                # Fill the gap until start_pos with 0xFF.
                fp.write(bytes([0xFF]) * (start_pos - fp.tell()))

                # Write ROM contents.
                fp.write(contents)

            # Each ROM slot is as big as the MAX_ROM_SIZE, even if the actual
            # ROM is smaller.
            start_pos += MAX_ROM_SIZE


if __name__ == "__main__":
    main()
