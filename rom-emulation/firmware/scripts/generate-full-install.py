#!/usr/bin/env python3
import argparse
import os.path
import struct
import subprocess
import tempfile
import json
from itertools import batched

BLOCK_SIZE = 0x1000  # i.e. the flash erase size
ALL_PERMISSIONS = {"secure": "rw", "nonsecure": "rw", "bootloader": "rw"}
NUM_SUPERBLOCKS = 16


class FlashImage:
    def __init__(self, size: int):
        assert size % BLOCK_SIZE == 0

        num_blocks = size // BLOCK_SIZE
        self.written_blocks_map = [False] * num_blocks

        empty_block_contents = b"\xff" * BLOCK_SIZE
        self.contents = bytearray(empty_block_contents * num_blocks)

    def set_contents(self, offset: int, data: bytes):
        assert offset >= 0

        for i, value in enumerate(data, offset):
            self.contents[i] = value
            self.written_blocks_map[i // BLOCK_SIZE] = True

    def save_to_bin(self, path: str):
        with open(path, "wb") as fp:
            fp.write(self.contents)

    def save_to_uf2(self, path: str):
        # Split populated data into 256-sized blocks.
        chunks: dict[int, bytes] = {}  # base address -> data
        for i, was_written in enumerate(self.written_blocks_map):
            if not was_written:
                continue
            block_bytes = self.contents[i * BLOCK_SIZE : (i + 1) * BLOCK_SIZE]
            for j, chunk_data in enumerate(batched(block_bytes, 256)):
                address = i * BLOCK_SIZE + j * 256
                chunks[address] = bytes(chunk_data)

        # Write them out.
        with open(path, "wb") as fp:
            for i, (address, chunk_data) in enumerate(chunks.items()):
                fp.write(
                    struct.pack(
                        "<IIIIIIII",
                        0x0A324655,  # First magic number.
                        0x9E5D5157,  # Second magic number.
                        0x00002000,  # Flags: only "family ID present".
                        0x10000000 + address,  # Target address.
                        256,  # Payload size.
                        i,  # Block number.
                        len(chunks),  # Total blocks.
                        0xE48BFF57,  # Family ID: "absolute".
                    )
                )
                fp.write(chunk_data.ljust(476, b"\xff"))
                fp.write(
                    struct.pack(
                        "<I",
                        0x0AB16F30,  # End magic number.
                    )
                )


def setup_partition_table(
    ab_size: int,
    data_size: int,
    dest_image: FlashImage,
    picotool_path: str,
) -> tuple[int, int, int]:
    assert ab_size % BLOCK_SIZE == 0
    assert data_size % BLOCK_SIZE == 0

    a_start = BLOCK_SIZE
    b_start = a_start + ab_size
    data_start = b_start + ab_size

    partition_table = {
        "unpartitioned": {
            "families": ["absolute"],
            "permissions": ALL_PERMISSIONS,
        },
        "partitions": [
            {
                "name": "Main A",
                "start": a_start | 0x10000000,
                "size": ab_size,
                "families": ["rp2350-arm-s"],
                "permissions": ALL_PERMISSIONS,
            },
            {
                "name": "Main B",
                "start": b_start,
                "size": ab_size,
                "families": ["rp2350-arm-s"],
                "permissions": ALL_PERMISSIONS,
                "link": ["a", 0],
            },
            {
                "name": "Data",
                "start": data_start,
                "size": data_size,
                "families": ["data"],
                "ignored_during_arm_boot": True,
                "ignored_during_riscv_boot": True,
                "no_reboot_on_uf2_download": True,
                "permissions": ALL_PERMISSIONS,
            },
        ],
    }

    with tempfile.TemporaryDirectory() as tmpdir:
        # Write the partition table in JSON format into a temporary file.
        json_path = os.path.join(tmpdir, "partition-table.json")
        with open(json_path, "wt", encoding="ascii") as json_fp:
            json.dump(partition_table, json_fp)

        # Assemble the corrisponding header.
        bin_path = os.path.join(tmpdir, "partition-table.bin")
        subprocess.check_call(
            [
                picotool_path,
                "partition",
                "create",
                json_path,
                bin_path,
                "--singleton",
            ]
        )

        # Copy the resulting bytes at the beginning of the destination flash:
        with open(bin_path, "rb") as bin_fp:
            bin_data = bin_fp.read()
        assert len(bin_data) <= a_start
        dest_image.set_contents(0, bin_data)

    return (a_start, b_start, data_start)


def main():
    parser = argparse.ArgumentParser(
        prog="generate-full-install",
        description="Generates a prepartitioned flash image to be flashed into the Pico.",
    )

    parser.add_argument(
        "--with-picotool",
        default="picotool",
        dest="picotool_path",
        metavar="/path/to/picotool",
        help='Optionally override the path to the "picotool" executable to use',
    )
    parser.add_argument(
        "--pico-binary",
        metavar="pico-image.bin",
        help="Binary program that will run on the Raspberry Pico 2 or 2 W",
        required=True,
    )
    parser.add_argument(
        "--output",
        metavar="flash-image.uf2",
        help="Output path for the resulting flash image",
        required=True,
    )

    args = parser.parse_args()

    # The Pico 2 and 2 W boards have 4 MiB of total flash memory.
    flash_image = FlashImage(4 * 1024 * 1024)

    # Partition the Pico's 4 MiB flash memory with this layout:
    # - Partition Table's own header: 1 block (first block)
    # - Partition A: 1.5 MiB minus 10 blocks
    # - Partition B: 1.5 MiB minus 10 blocks
    # - Partition DATA: 1 MiB + 18 blocks
    # - Workaround for errata E10: 1 block (last block, in unpartitioned space)
    ab_size = 1536 * 1024 - 10 * BLOCK_SIZE
    data_size = 1024 * 1024 + 18 * BLOCK_SIZE
    a_start, b_start, data_start = setup_partition_table(
        ab_size=ab_size,
        data_size=data_size,
        dest_image=flash_image,
        picotool_path=args.picotool_path,
    )

    # Fill partition A with the Pico program.
    with open(args.pico_binary, "rb") as pico_binary_fp:
        pico_binary_data = pico_binary_fp.read()
    if len(pico_binary_data) > ab_size:
        exit(f"The Pico binary does not fit in the A/B partition ({ab_size} B)")
    flash_image.set_contents(a_start, pico_binary_data)

    # Erase the first block in partition B to prevent it from booting.
    flash_image.set_contents(b_start, b"\xff" * BLOCK_SIZE)

    # Clear the superblocks in partition DATA.
    total_superblocks_size = BLOCK_SIZE * NUM_SUPERBLOCKS
    flash_image.set_contents(data_start, b"\xff" * total_superblocks_size)

    # Write the output file.
    flash_image.save_to_uf2(args.output)


if __name__ == "__main__":
    main()
