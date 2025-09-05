#!/usr/bin/env python3
import sys
from pathlib import Path

# This script patches the linker script provided by the pico SDK to split the
# RAM memory region in two halves.
#
# The RP2350 datasheet (section "2.2.3. SRAM") explains that there are
# four ranges of SRAM:
#   0x20000000-0x20040000    Striped among banks SRAM0-3
#   0x20040000-0x20080000    Striped among banks SRAM4-7
#   0x20080000-0x20081000    Backed by bank SRAM8
#   0x20081000-0x20082000    Backed by bank SRAM9
#
# In order to avoid contention between core0 and core1, we need to ensure that
# the ROM image is loaded into SRAM4-7 and all the other variables into SRAM0-3.
#
# However, the SDK-provided linker script doesn't differentiate between the
# first two regions and simply defines one big "RAM" range covering both:
#
# src/rp2_common/pico_crt0/rp2350/memmap_default.ld:
# MEMORY
# {
#     INCLUDE "pico_flash_region.ld"
#     RAM(rwx) : ORIGIN =  0x20000000, LENGTH = 512k
#     SCRATCH_X(rwx) : ORIGIN = 0x20080000, LENGTH = 4k
#     SCRATCH_Y(rwx) : ORIGIN = 0x20081000, LENGTH = 4k
# }
#
# Note: 0x20000000+512k == 0x20080000.
#
# This script patchs the SDK-provided linker script so that:
#  - Two 256k-sized ranges "RAM" and "RAM2" are defined instead.
#  - The "ram2_uninitialized_data" section ends up being allocated in "RAM2".


sdk_path = Path(sys.argv[1])
input_path = sdk_path.joinpath("src/rp2_common/pico_crt0/rp2350/memmap_default.ld")
output_path = Path(sys.argv[2])

# Load the original linker script from the SDK.
ldscript = input_path.read_text()


def patch(old: str, new: str):
    global ldscript
    pos_start = ldscript.index(old)
    pos_end = pos_start + len(old)
    ldscript = ldscript[:pos_start] + new + ldscript[pos_end:]


patch(
    old="""
    RAM(rwx) : ORIGIN =  0x20000000, LENGTH = 512k""",
    new="""
    RAM(rwx) : ORIGIN =  0x20000000, LENGTH = 256k
    RAM2(rwx) : ORIGIN =  0x20040000, LENGTH = 256k""",
)

patch(
    old="""
    .uninitialized_data (NOLOAD): {
        . = ALIGN(4);
        *(.uninitialized_data*)
    } > RAM""",
    new="""
    .uninitialized_data (NOLOAD): {
        . = ALIGN(4);
        *(.uninitialized_data*)
    } > RAM

    .ram2_uninitialized_data (NOLOAD): {
        *(.ram2_uninitialized_data*)
    } > RAM2""",
)

output_path.write_text(ldscript)
