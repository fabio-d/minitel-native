#!/usr/bin/env python3
import argparse
import enum
import sys
from pathlib import Path

# This script takes a comma-separated ordered list of the 16 CPU-side bus line
# names, and generates the corresponding functions to permute them to/from
# Pico's GPIOs, in order from 0 to 15.
#
# Due to limitations of the PIO programs:
# - AD lines must be clustered next to each other
# - ALE and PSEN (and WR and RD, if present) must be consecutive


class BusSpecialFunction(enum.Enum):
    NOPEN = enum.auto()
    BUSEN = enum.auto()
    RST = enum.auto()
    ALE = enum.auto()
    PSEN = enum.auto()
    WR = enum.auto()
    RD = enum.auto()


class BusLine:
    def __init__(self, gpioid: int, busid: int | BusSpecialFunction):
        if isinstance(busid, BusSpecialFunction):
            assert (16 <= gpioid <= 22) or (26 <= gpioid <= 28)
        else:
            assert 0 <= gpioid <= 15
            assert 0 <= busid <= 15
        self.gpioid = gpioid
        self.busid = busid

    @property
    def gpioname(self) -> str:
        return f"GPIO{self.gpioid}"

    @property
    def busname(self) -> str:
        if isinstance(self.busid, BusSpecialFunction):
            return self.busid.name
        elif self.busid < 8:
            return f"AD{self.busid}"
        else:
            return f"A{self.busid}"

    def __repr__(self):
        return f"{self.gpioname}<>{self.busname}"


def generate_permutation_function(
    func_name: str,
    data_type: str,
    mapping: list[tuple[int, int]],
) -> str:
    code = f"inline {data_type} {func_name}({data_type} inval) {{\n"
    code += f"  {data_type} outval = 0;\n"
    for src, dst in mapping:
        code += f"  if (inval & (1 << {src})) outval |= 1 << {dst};\n"
    code += f"  return outval;\n"
    code += f"}}\n"
    return code


def busid2gpioid(
    buslines: list[BusLine], busid: int | BusSpecialFunction
) -> int:
    for busline in buslines:
        if busline.busid == busid:
            return busline.gpioid
    raise KeyError


parser = argparse.ArgumentParser()
# required arguments
parser.add_argument("input_busnames")
parser.add_argument("output_path", type=Path)
# optional features
parser.add_argument("--with-bus-switch", action="store_true")
parser.add_argument("--parking-mechanism", choices=("NOP", "RST"))
parser.add_argument("--with-ram-controls", action="store_true")

args = parser.parse_args()

# Convert the array input strings into just numbers ("ADn" or "An" -> n) or
# values from the BusSpecialFunction enum.
busname_to_busid = {
    "ALE": BusSpecialFunction.ALE,
    "PSEN": BusSpecialFunction.PSEN,
}
if args.with_bus_switch:
    busname_to_busid["BUSEN"] = BusSpecialFunction.BUSEN
    match args.parking_mechanism:
        case "NOP":
            busname_to_busid["NOPEN"] = BusSpecialFunction.NOPEN
        case "RST":
            busname_to_busid["RST"] = BusSpecialFunction.RST
        case _:
            exit("Need parking mechanism to use before bus switch is turned on")
if args.with_ram_controls:
    busname_to_busid["WR"] = BusSpecialFunction.WR
    busname_to_busid["RD"] = BusSpecialFunction.RD
for n in [0, 1, 2, 3, 4, 5, 6, 7]:
    busname_to_busid[f"AD{n}"] = n
for n in [8, 9, 10, 11, 12, 13, 14, 15]:
    busname_to_busid[f"A{n}"] = n
buslines = [
    BusLine(gpioid, busname_to_busid.pop(busname))
    for gpioid, busname in enumerate(args.input_busnames.split(","))
    if busname != ""
]
if len(busname_to_busid) != 0:
    exit("Not all bus lines have been mapped!")

# Emit all mappings as constants.
mappings = "".join(
    f"inline constexpr uint PIN_{busline.busname} = {busline.gpioid}; // {busline}\n"
    for busline in buslines
)

# Filter AD and A lines only. Entries are stored in order of increasing gpioid.
a_buslines = [busline for busline in buslines if isinstance(busline.busid, int)]

# Filter AD0-7 only. Entries are stored in order of increasing gpioid.
ad_buslines = [busline for busline in a_buslines if busline.busid < 8]

# Emit bus lines' names in GPIO order.
mappings += "inline const char PIN_ADDR_ALL_NAMES[] =\n"
mappings += '    "%s";\n' % "|".join(busline.busname for busline in a_buslines)

# Verify that the AD lines are clustered next to each other.
if ad_buslines[0].gpioid + 7 != ad_buslines[7].gpioid:
    exit("The AD lines must be mapped to consecutive GPIOs")
mappings += "inline constexpr uint PIN_AD_BASE = %d;\n" % ad_buslines[0].gpioid

# Verify that ALE and PSEN are consecutive.
ale_gpioid = busid2gpioid(buslines, BusSpecialFunction.ALE)
psen_gpioid = busid2gpioid(buslines, BusSpecialFunction.PSEN)
if ale_gpioid + 1 != psen_gpioid:
    exit("The ALE and PSEN lines must consecutive")
if args.with_ram_controls:
    wr_gpioid = busid2gpioid(buslines, BusSpecialFunction.WR)
    rd_gpioid = busid2gpioid(buslines, BusSpecialFunction.RD)
    if ale_gpioid + 2 != wr_gpioid or ale_gpioid + 3 != rd_gpioid:
        exit("The ALE, PSEN, WR and RD lines must be consecutive")

pin_map_data = "// Permutation function for data bits.\n"
pin_map_data += generate_permutation_function(
    "pin_map_data",
    "uint8_t",
    [(busline.busid, i) for i, busline in enumerate(ad_buslines)],
)

pin_map_address = "// Permutation function for address bits.\n"
pin_map_address += "//\n"
pin_map_address += "// Every address bit ends up at its GPIO position.\n"
pin_map_address += generate_permutation_function(
    "pin_map_address",
    "uint16_t",
    [(busline.busid, busline.gpioid) for busline in a_buslines],
)
pin_map_address += generate_permutation_function(
    "pin_map_address_inverse",
    "uint16_t",
    [(busline.gpioid, busline.busid) for busline in a_buslines],
)

with args.output_path.open("wt") as fp:
    print("#ifndef PIN_MAP_H", file=fp)
    print("#define PIN_MAP_H", file=fp)
    print("", file=fp)
    print("#include <pico/types.h>", file=fp)
    print("#include <stdint.h>", file=fp)
    print("", file=fp)
    print(mappings, file=fp)
    print(pin_map_data, file=fp)
    print(pin_map_address, file=fp)
    print("#endif", file=fp)
