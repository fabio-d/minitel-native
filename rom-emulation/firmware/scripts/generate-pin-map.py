#!/usr/bin/env python3
import sys
from itertools import batched
from pathlib import Path

# This script takes a comma-separated ordered list of the 16 CPU-side bus line
# names, and generates the corresponding functions to permute them to/from
# Pico's GPIOs, in order from 0 to 15. Due to limitations of the PIO peripheral,
# AD lines must be clustered in two groups of 4 GPIOs each.


class BusLine:
    def __init__(self, gpioid: int, busid: int):
        assert gpioid < 16 and busid < 16
        self.gpioid = gpioid
        self.busid = busid

    @property
    def gpioname(self) -> str:
        return f"GPIO{self.gpioid}"

    @property
    def busname(self) -> str:
        if self.busid < 8:
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


input_busnames = sys.argv[1]
output_path = Path(sys.argv[2])

# Convert the array input strings into just numbers ("ADn" or "An" -> n).
busname_to_busid = dict()
for n in [0, 1, 2, 3, 4, 5, 6, 7]:
    busname_to_busid[f"AD{n}"] = n
for n in [8, 9, 10, 11, 12, 13, 14, 15]:
    busname_to_busid[f"A{n}"] = n
buslines = [
    BusLine(gpioid, busname_to_busid.pop(busname))
    for gpioid, busname in enumerate(input_busnames.split(","))
]
if len(busname_to_busid) != 0:
    exit("Not all bus lines have been mapped!")

# Emit all mappings as constants.
mappings = "".join(
    f"inline constexpr uint PIN_{busline.busname} = {busline.gpioid}; // {busline}\n"
    for busline in buslines
)
mappings += "inline constexpr uint PIN_NOPEN = 17;\n"
mappings += "inline constexpr uint PIN_BUSEN = 18;\n"
mappings += "inline constexpr uint PIN_ALE = 19;\n"
mappings += "inline constexpr uint PIN_PSEN = 20;\n"

# Emit bus lines' names in GPIO order.
mappings += "inline const char PIN_ADDR_ALL_NAMES[] =\n"
mappings += '    "%s";\n' % "|".join(busline.busname for busline in buslines)

# Filter AD0-7 only. Entries are stored in order of increasing gpioid.
ad_buslines = [busline for busline in buslines if busline.busid < 8]

# Identify the first 4 AD lines, that will be managed by state machine A, and
# the remaining other 4 AD lines, that will be managed by state machine B. Each
# group of 4 lines must be mapped to consecutive GPIOs.
(ad_group_a, ad_group_b) = batched(ad_buslines, 4)
if ad_group_a[0].gpioid + 3 != ad_group_a[3].gpioid:
    exit("The first group of 4 AD lines is not mapped to consecutive GPIOs")
if ad_group_b[0].gpioid + 3 != ad_group_b[3].gpioid:
    exit("The second group of 4 AD lines is not mapped to consecutive GPIOs")

sm_a_parameters = f"// State machine A manages {ad_group_a}.\n"
sm_a_parameters += "inline constexpr uint PIN_AD_BASE_A = %d;\n" % (
    ad_group_a[0].gpioid  # lowest gpioid managed by state machine A
)
sm_b_parameters = f"// State machine B manages {ad_group_b}.\n"
sm_b_parameters += "inline constexpr uint PIN_AD_BASE_B = %d;\n" % (
    ad_group_b[0].gpioid  # lowest gpioid managed by state machine B
)

pin_map_data = "// Permutation function for data bits.\n"
pin_map_data += "//\n"
pin_map_data += "// State machine A data bits end up at positions 0-3.\n"
pin_map_data += "// State machine B data bits end up at positions 4-7.\n"
pin_map_data += generate_permutation_function(
    "pin_map_data",
    "uint8_t",
    [(busline.busid, i) for i, busline in enumerate(ad_group_a + ad_group_b)],
)

pin_map_address = "// Permutation function for address bits.\n"
pin_map_address += "//\n"
pin_map_address += "// Every address bit ends up at its GPIO position.\n"
pin_map_address += generate_permutation_function(
    "pin_map_address",
    "uint16_t",
    [(busline.busid, busline.gpioid) for busline in buslines],
)
pin_map_address += generate_permutation_function(
    "pin_map_address_inverse",
    "uint16_t",
    [(busline.gpioid, busline.busid) for busline in buslines],
)

with output_path.open("wt") as fp:
    print("#ifndef PIN_MAP_H", file=fp)
    print("#define PIN_MAP_H", file=fp)
    print("", file=fp)
    print("#include <pico/types.h>", file=fp)
    print("#include <stdint.h>", file=fp)
    print("", file=fp)
    print(mappings, file=fp)
    print(sm_a_parameters, file=fp)
    print(sm_b_parameters, file=fp)
    print(pin_map_data, file=fp)
    print(pin_map_address, file=fp)
    print("#endif", file=fp)
