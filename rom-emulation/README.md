# ROM emulation, or how to try out Minitel ROMs with ease

## Introduction

> [!CAUTION]
> Minitels contain a cathode-ray tube (CRT) monitor. CRT monitors are fragile,
> driven by lethally-high voltages (tens of thousands of volts) and contain
> toxic materials.
>
> While the steps for replacing the Minitel's ROM do not involve disassembling
> the CRTs, keep in mind that you will operate in very close proximity to
> high-voltage parts, that may
> **retain their charge even if unplugged from the AC for extended periods of time**
> (even months), because CRTs act like a big capacitors.
>
> Most Minitels tend to have a similar internal structure: a low-voltage logic
> board (usually below the CRT, closer to the keyboard) and a separate
> high-voltage board for driving the CRT and power distribution (usually on the
> side, close to the brightness knob). You are not the first person that wants
> to mod a Minitel: do some research first and look at pictures and instructions
> posted on the Internet about your specific Minitel model.
>
> It is essential and necessary to familiarize with the
> [**SAFETY HAZARDS** of CRTs](https://www.ifixit.com/Troubleshooting/Television/CRT+Repair+Risks+and+Safety/482706)
> before opening the case of your Minitel, and remember that the high voltages
> are not just in the CRT, but also in the circuits that drive it.
>
> Always unplug the AC when getting close to any of the Minitel's boards,
> follow all the precautions and acknowledge all the risks you are taking. This
> page was not written by a CRT expert: do your own research and decide on your
> own. I take no responsibility. DO NOT PROCEED IF YOU DO NOT KNOW WHAT YOU ARE
> DOING!

> [!WARNING]
> The low-voltage logic board usually also contains the modem, and the phone
> cord terminates into it. The incoming voltages from the phone network can be
> dangerous too. Remember to disconnect the phone plug from the wall socket.

The Minitel CPU, like any other processor, needs to fetch (i.e. read) the next
instruction to execute, at the beginning of every cycle, from some kind of
memory.

Specifically, the Minitel CPU is based on the
[Harvard](https://en.wikipedia.org/wiki/Harvard_architecture) architecture. In
this architecture, data is stored in RAM, but the instructions are stored in a
separate memory. In the Minitels, this memory is the ROM. One of the advantages
of this design is that it needs neither bootloaders nor large RAMs to "load" the
program into at startup (in fact, the Minitel only has 256 bytes of RAM): the
CPU can just fetch the instructions directly from the ROM, at every cycle, right
when they are needed.

The Minitels targeted by this project have either
[an 8052 or an 8032 CPU](https://en.wikipedia.org/wiki/Intel_MCS-51). These two
CPU models are identical, except for the presence of an integrated 8 KiB ROM in
the 8052, that is not present in the 8032. Therefore, there may or may not be an
external ROM chip containing the program memory. Luckily, it seems to be the
case that, even when there is an 8052 CPU without an external ROM chip, the
Minitel boards still have a functional unpopulated socket where an external ROM
can be installed (and the capability to make the CPU to use it, in place of the
integrated one, with a simple non-invasive hardware mod).

In conclusion, in order to replace the ROM of the Minitel:
* If the Minitel came with an external ROM chip, just replace it with a
  different one containing the new program.
* In the case of an 8052-based Minitel that had no external ROM chip to begin
  with, install the new ROM chip in the unpopulated socket and apply the mod to
  make the CPU boot from it. Refer to the board-specific links in the next
  section for details.

This page describes the steps to perform the above operations in the supported
Minitel models, with one twist: rather than installing a new real ROM chip, we
are going to install a custom
[Raspberry Pico](https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html)-based
board that acts like a ROM chip ("emulates a ROM").

## Installation steps

First of all, confirm that your Minitel model is supported and get the
corresponding adapter board:
* RTIC Minitel 1 (NFZ 330) is supported by
  [`board_nfz330_nfz400`](board_nfz330_nfz400/).
* Philips Minitel 2 (NFZ 400) is supported by
  [`board_nfz330_nfz400`](board_nfz330_nfz400/).

Once you have identified what adapter board to use, get a
[Raspberry Pico 2 or Pico 2 W](https://www.raspberrypi.com/products/raspberry-pi-pico-2/)
(**not** Pico 1) and flash it as described in the
[`firmware`](firmware/README.md) subdirectory.

Finally, insert the Pico in the adapter board and the adapter board in the
Minitel's logic board, as shown in the adapter-specific instructions.

## Working with the hardware design files

The hardware design files in the `board_*` subdirectories were created with
[KiCad](https://www.kicad.org/), an open-source printed circuit board (PCB)
design tool.

In KiCad's PCB Editor, the footprints were annotated with
[LCSC/JLCPCB part numbers](https://jlcpcb.com/parts) using the
[kicad-jlcpcb-tools](https://github.com/Bouni/kicad-jlcpcb-tools) plugin.

The same plugin was also used to export the production files, for both PCB
fabrication (Gerber files) and assembly (BOM and pick-and-place).
