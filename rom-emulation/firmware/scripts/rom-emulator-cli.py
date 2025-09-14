#!/usr/bin/env python3
import argparse
import binascii
import os.path
import random
import struct
import serial
import sys

PACKET_MAGIC_BEGIN = 0x5CA7
PACKET_MAGIC_END = 0x6DE1

PACKET_TYPE_EMULATOR_PING = 0
PACKET_TYPE_EMULATOR_TRACE = 1
PACKET_TYPE_EMULATOR_BOOT = 2
PACKET_TYPE_EMULATOR_WRITE_BEGIN = 3
PACKET_TYPE_EMULATOR_WRITE_DATA = 4
PACKET_TYPE_EMULATOR_WRITE_END = 5
PACKET_TYPE_REPLY_XOR_MASK = 0x80

MAX_ROM_SIZE = 64 * 1024
TRANSFER_STEP = 128


# Sends a request packet and waits for the reply.
def transfer_packet(
    serial_port: serial.Serial, packet_type: int, data: bytes
) -> bytes:
    header = struct.pack("<HHB", PACKET_MAGIC_BEGIN, len(data), packet_type)
    checksum = binascii.crc_hqx(header[2:] + data, 0)  # excluding MAGIC_BEGIN.
    footer = struct.pack("<HH", checksum, PACKET_MAGIC_END)

    # Wrap the outgoing packet with header and footer.
    serial_port.write(header + data + footer)

    # Expect the reply's header. Its size matches the one in the request.
    r_header = serial_port.read(len(header))
    if len(r_header) != len(header):
        raise TimeoutError
    r_magic_begin, r_length, r_packet_type = struct.unpack("<HHB", r_header)
    if (
        r_magic_begin != PACKET_MAGIC_BEGIN
        or r_packet_type != packet_type ^ PACKET_TYPE_REPLY_XOR_MASK
    ):
        raise RuntimeError("Received data does not look like a reply packet")

    # Expect the reply's contents.
    r_data = serial_port.read(r_length)
    if len(r_data) != r_length:
        raise TimeoutError
    r_checksum_expected = binascii.crc_hqx(r_header[2:] + r_data, 0)

    # Expect the reply's footer. Its size matches the one in the request.
    r_footer = serial_port.read(len(footer))
    if len(r_footer) != len(footer):
        raise TimeoutError
    r_checksum_actual, r_magic_end = struct.unpack("<HH", r_footer)
    if (
        r_checksum_expected != r_checksum_actual
        or r_magic_end != PACKET_MAGIC_END
    ):
        raise RuntimeError("Received data does not look like a reply packet")

    return r_data


def do_ping(serial_port: serial.Serial, args: argparse.Namespace):
    # If we are here, the device has already been pinged successfully by main().
    # Let's just print a message and exit.
    print("Ping success!", file=sys.stderr)


def do_trace(serial_port: serial.Serial, args: argparse.Namespace):
    reply = transfer_packet(serial_port, PACKET_TYPE_EMULATOR_TRACE, b"")
    if len(reply) == 0:
        print(
            "No ROM addresses were accessed during the sampling interval. "
            "Is the CPU running?",
            file=sys.stderr,
        )
    else:
        for (addr,) in struct.iter_unpack("<H", reply):
            print(f"{addr:#06x}", file=sys.stderr)


def do_boot(serial_port: serial.Serial, args: argparse.Namespace):
    reply = transfer_packet(
        serial_port,
        PACKET_TYPE_EMULATOR_BOOT,
        struct.pack("<B", args.slot),
    )
    if reply == b"OK":
        print("Boot command succeeded.", file=sys.stderr)
    else:
        exit("Boot command failed.")


def do_store(serial_port: serial.Serial, args: argparse.Namespace):
    # Determine the ROM name that will be displayed in the menu.
    if args.label is not None:
        name = args.label
    else:
        name = os.path.basename(args.rom_file.name)

    # Read ROM binary, up to MAX_ROM_SIZE+1 so we can determine if the file is
    # too big to be a real ROM.
    data = b""
    while chunk := args.rom_file.read(MAX_ROM_SIZE + 1 - len(data)):
        data += chunk
    if len(data) == 0 or len(data) > MAX_ROM_SIZE:
        exit(f"Invalid ROM size: {len(data)}")

    transfer_packet(
        serial_port,
        PACKET_TYPE_EMULATOR_WRITE_BEGIN,
        struct.pack("<B", args.slot) + name.encode()[:126],
    )

    for i in range(0, len(data), TRANSFER_STEP):
        percent = round(100 * i / len(data))
        print(f"Progress: {i}/{len(data)} bytes ({percent} %)", file=sys.stderr)
        reply = transfer_packet(
            serial_port,
            PACKET_TYPE_EMULATOR_WRITE_DATA,
            data[i : i + TRANSFER_STEP],
        )
        if reply != b"OK":
            exit("Write failed.")

    print(f"Progress: {len(data)}/{len(data)} bytes (100 %)", file=sys.stderr)
    reply = transfer_packet(serial_port, PACKET_TYPE_EMULATOR_WRITE_END, b"")
    if reply != b"OK":
        exit("Write failed.")
    print("Store command succeeded.", file=sys.stderr)

    if args.boot:
        do_boot(serial_port, argparse.Namespace(slot=args.slot))


# Parses the --slot argument.
#
# Note: the name of this function is shown in argparse's error message when the
# user supplies an invalid value.
def SLOT(text: str) -> int:
    value = int(text, 16)
    if value < 0 or value > 15:
        raise ValueError
    return value


def main():
    parser = argparse.ArgumentParser(
        prog="rom-emulator-cli",
        description="Interact with the Minitel ROM emulator over serial port.",
    )

    parser.add_argument(
        "-s",
        "--serial",
        metavar="PORT_NAME",
        help=(
            "PySerial port name for connecting to the ROM emulator. Refer to "
            "PySerial's serial.serial_for_url() documentation for the format."
        ),
        required=True,
    )

    subparsers = parser.add_subparsers(
        metavar="COMMAND",
        required=True,
    )

    parser_ping = subparsers.add_parser(
        name="ping",
        help="Verifies that the ROM emulator is responding.",
    )
    parser_ping.set_defaults(func=do_ping)

    parser_trace = subparsers.add_parser(
        name="trace",
        help="Prints the most recently accessed ROM addresses.",
    )
    parser_trace.set_defaults(func=do_trace)

    parser_boot = subparsers.add_parser(
        name="boot",
        help="Starts the ROM stored in flash memory.",
    )
    parser_boot.add_argument(
        "-n",
        "--slot",
        type=SLOT,
        help="ROM slot number to boot from (hex value between 0 and F).",
        required=True,
    )
    parser_boot.set_defaults(func=do_boot)

    parser_store = subparsers.add_parser(
        name="store",
        help="Stores a new ROM into flash memory.",
    )
    parser_store.add_argument(
        "-n",
        "--slot",
        type=SLOT,
        help="ROM slot number to store into (hex value between 0 and F).",
        required=True,
    )
    parser_store.add_argument(
        "-l",
        "--label",
        help="set the title displayed in the menu (default: the filename).",
    )
    parser_store.add_argument(
        "-b",
        "--boot",
        help="boot immediately after.",
        action="store_true",
    )
    parser_store.add_argument(
        "rom_file",
        metavar="rom.bin",
        type=argparse.FileType("rb"),
        help="ROM binary file.",
    )
    parser_store.set_defaults(func=do_store)

    args = parser.parse_args()

    # Initialize the link to the ROM emulator.
    serial_port = serial.serial_for_url(
        args.serial,
        baudrate=2400,
        inter_byte_timeout=5,  # inactivity timeout (only applies to reads)
    )

    # Flush the input buffer, in case there is any stale data, and then send
    # a ping request with a random fresh payload to verify that the ROM emulator
    # is responding.
    serial_port.read_all()  # flush
    ping_data = random.randbytes(8)
    ping_reply = transfer_packet(
        serial_port,
        PACKET_TYPE_EMULATOR_PING,
        ping_data,
    )
    if ping_reply != ping_data:
        exit("Device not responding")

    # Run the requested command.
    args.func(serial_port, args)


if __name__ == "__main__":
    main()
