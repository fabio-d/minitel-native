#!/usr/bin/env python3
import argparse
import binascii
import os.path
import random
import struct
import serial
import sys

PROTOCOL_TCP_PORT = 3759

PACKET_MAGIC_BEGIN = 0x5CA7
PACKET_MAGIC_END = 0x6DE1

PACKET_TYPE_EMULATOR_PING = 0
PACKET_TYPE_EMULATOR_TRACE = 1
PACKET_TYPE_EMULATOR_BOOT = 2
PACKET_TYPE_EMULATOR_WRITE_BEGIN = 3
PACKET_TYPE_EMULATOR_WRITE_DATA = 4
PACKET_TYPE_EMULATOR_WRITE_END = 5
PACKET_TYPE_EMULATOR_WIRELESS_CONFIG = 6
PACKET_TYPE_EMULATOR_OTA_BEGIN = 7
PACKET_TYPE_EMULATOR_OTA_DATA = 8
PACKET_TYPE_EMULATOR_OTA_END = 9
PACKET_TYPE_EMULATOR_ERASE = 10
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


def do_erase(serial_port: serial.Serial, args: argparse.Namespace):
    reply = transfer_packet(
        serial_port,
        PACKET_TYPE_EMULATOR_ERASE,
        struct.pack("<B", args.slot),
    )
    if reply == b"OK":
        print("Erase command succeeded.", file=sys.stderr)
    else:
        exit("Erase command failed.")


def do_wl_set(serial_port: serial.Serial, args: argparse.Namespace):
    ssid = args.ssid.encode("utf-8")
    psk = args.psk.encode("utf-8")
    if not (0 < len(ssid) <= 32):
        exit("Wireless SSID has an invalid length")
    if len(psk) != 0:  # open networks need no password (i.e. empty string)
        if not (8 <= len(psk) <= 63):
            exit("Wireless password has an invalid length")
    reply = transfer_packet(
        serial_port,
        PACKET_TYPE_EMULATOR_WIRELESS_CONFIG,
        ssid.ljust(32, b"\0") + psk.ljust(63, b"\0"),
    )
    if reply == b"OK":
        print("Wireless configuration succeeded.", file=sys.stderr)
    else:
        exit("Wireless is not supported by the target.")


def do_wl_unset(serial_port: serial.Serial, args: argparse.Namespace):
    reply = transfer_packet(
        serial_port,
        PACKET_TYPE_EMULATOR_WIRELESS_CONFIG,
        b"\0" * (32 + 63),
    )
    if reply == b"OK":
        print("Wireless configuration succeeded.", file=sys.stderr)
    else:
        exit("Wireless is not supported by the target.")


def do_ota(serial_port: serial.Serial, args: argparse.Namespace):
    data = bytearray()  # firmware image contents
    blocks_counter = 0
    blocks_total = None

    while block_data := args.firmware_file.read(512):
        (
            magic_start_0,
            magic_start_1,
            flags,
            target_addr,
            payload_size,
            block_number,
            total_blocks,
            file_size,
            payload,
            magic_end,
        ) = struct.unpack("<IIIIIIII476sI", block_data)

        if (
            magic_start_0,
            magic_start_1,
            magic_end,
        ) != (
            0x0A324655,
            0x9E5D5157,
            0x0AB16F30,
        ):
            exit("Invalid UF2 file")

        if (flags & 0x2000) == 0 or file_size != 0xE48BFF59:
            continue  # Skip blocks whose family-id is not "rp2350-arm-s".

        if blocks_total is None:  # Get blocks_total from the first valid block.
            blocks_total = total_blocks

        if block_number != blocks_counter or block_number >= blocks_total:
            exit("Invalid UF2 block number detected")

        image_offset = target_addr - 0x10000000
        if image_offset != len(data):
            raise NotImplementedError("Support for gaps in the firmware image")

        data.extend(payload[:payload_size])
        blocks_counter += 1

    transfer_packet(serial_port, PACKET_TYPE_EMULATOR_OTA_BEGIN, b"")

    for i in range(0, len(data), TRANSFER_STEP):
        percent = round(100 * i / len(data))
        print(f"Progress: {i}/{len(data)} bytes ({percent} %)", file=sys.stderr)
        reply = transfer_packet(
            serial_port,
            PACKET_TYPE_EMULATOR_OTA_DATA,
            data[i : i + TRANSFER_STEP],
        )
        if reply != b"OK":
            exit("OTA failed.")

    print(f"Progress: {len(data)}/{len(data)} bytes (100 %)", file=sys.stderr)
    reply = transfer_packet(serial_port, PACKET_TYPE_EMULATOR_OTA_END, b"")
    if reply != b"OK":
        exit("OTA failed.")
    print("OTA command succeeded.", file=sys.stderr)
    print("The new firmware will be used at the next boot.", file=sys.stderr)


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
        description="Interact with the Minitel ROM emulator over serial port or TCP.",
    )

    serial_group = parser.add_mutually_exclusive_group(required=True)
    serial_group.add_argument(
        "-s",
        "--serial",
        metavar="PORT_NAME",
        help=(
            "PySerial port name for connecting to the ROM emulator. Refer to "
            "PySerial's serial.serial_for_url() documentation for the format."
        ),
    )
    serial_group.add_argument(
        "-t",
        "--tcp-host",
        metavar="HOST",
        help=f'shortcut for PySerial URL "socket://HOST:{PROTOCOL_TCP_PORT}".',
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
        help="Starts a ROM stored in flash memory.",
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
        epilog=(
            "Note: the previously stored ROM at the selected slot will be "
            "implicitly erased, if present, without warning."
        ),
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

    parser_erase = subparsers.add_parser(
        name="erase",
        help="Deletes a ROM stored in flash memory.",
    )
    parser_erase.add_argument(
        "-n",
        "--slot",
        type=SLOT,
        help="ROM slot number to erase (hex value between 0 and F).",
        required=True,
    )
    parser_erase.set_defaults(func=do_erase)

    parser_wl_set = subparsers.add_parser(
        name="wl-set",
        help="Configures and enables the wireless client interface.",
    )
    parser_wl_set.add_argument(
        "ssid",
        metavar="NETWORK_NAME",
        help="Wireless network name to connect to",
    )
    parser_wl_set.add_argument(
        "psk",
        metavar="PASSWORD",
        help="Wireless network password (empty string for open networks)",
    )
    parser_wl_set.set_defaults(func=do_wl_set)

    parser_wl_unset = subparsers.add_parser(
        name="wl-unset",
        help="Disables the wireless client interface.",
    )
    parser_wl_unset.set_defaults(func=do_wl_unset)

    parser_ota = subparsers.add_parser(
        name="ota",
        help="Updates the ROM emulator's own firmware.",
    )
    parser_ota.add_argument(
        "firmware_file",
        metavar="rom-emulator-update-only.uf2",
        type=argparse.FileType("rb"),
        help="RP2350 firmware UF2 file.",
    )
    parser_ota.set_defaults(func=do_ota)

    args = parser.parse_args()

    # Initialize the link to the ROM emulator.
    if args.tcp_host is not None:
        args.serial = f"socket://{args.tcp_host}:{PROTOCOL_TCP_PORT}"
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
