#!/usr/bin/env python3
import argparse
import struct

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
        "--output",
        metavar="PATH",
        help="Output path for the resulting data partition image",
        required=True,
    )

    args = parser.parse_args()

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
        # Write the superblock.
        for contents, name in roms:
            fp.write(
                struct.pack(
                    "<I127p",
                    0xFFFFFFFF if contents is None else len(contents),
                    name.encode(),  # TODO: encode nonstandard characters too
                )
            )

        # Store ROM contents at the corresponding locations.
        start_pos = 0x1000
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
