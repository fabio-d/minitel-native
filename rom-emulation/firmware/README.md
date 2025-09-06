# Firmware for the ROM Emulation board

## Building from source

Download the Pico SDK (tested with version 2.2.0) and the other prerequisites:
```shell
$ sudo apt install build-essential cmake gcc-arm-none-eabi \
    libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib python3

# The SDK can be downloaded into any directory. The next commands assume that
# it has been cloned into the home directory and, therefore, the resulting
# path is ~/pico-sdk.
$ git clone https://github.com/raspberrypi/pico-sdk.git --branch 2.2.0 \
    --recurse-submodules
```

Configure and build the project:
```shell
# Run these commands in the "firmware" directory (same as this README.md file).
$ mkdir build/
$ cd build/
$ cmake .. \
    -DPICO_SDK_PATH=~/pico-sdk \
    -DPICO_BOARD=<pico2|pico2_w> \
    -DMINITEL_MODEL=<nfz330|nfz400|justrom:...> \
    -DOPERATING_MODE=embedded -DEMBED_ROM_FILE=/path/to/rom.bin
$ make
```
where:
* `PICO_BOARD` is the Pico board model that will run the ROM emulation software.
* `MINITEL_MODEL` is the Minitel model that the ROM emulator will be used with:
  * `nfz330`: RTIC Minitel 1 in conjuction with `board_nfz330_nfz400`.
  * `nfz400`: Philips Minitel 2 in conjuction with `board_nfz330_nfz400`.
  * There is also some limited support for custom hardware boards and/or other
    Minitels: it is possible to pass `justrom:` followed by a comma-separated
    list of names of bus lanes connected to GPIOs from 0 to 15. See the
    [generate-pin-map.py](scripts/generate-pin-map.py) script for details.
* `OPERATING_MODE` selects what level of features should be enabled:
  * If set to `embedded`, `EMBED_ROM_FILE` should be set to path to the ROM file
    to be emulated. No other features, in addition to just serving the ROM, will
    be enabled.
  * Other modes will be added in the future.

## Installation

Connect the Pico's USB port to the computer while keeping the `BOOTSEL` button
pressed, and a new virtual disk drive will appear to be connected. Copy
`build/rom-emulator-embedded.uf2` into it. The disk drive will disconnect at the
end of the process and the Pico's on-board LED will start to blink, indicating
that the ROM emulation software is running.

## Client protocol

The [`scripts/rom-emulator-cli.py`](scripts/rom-emulator-cli.py) program can be
used to interact with the firmware while it is running on the Pico.

It supports the following connection channels:
* The Pico's own USB serial port (usage: `rom-emulator-cli.py -s /dev/ttyACM0`).

Available commands:
* `ping`: verifies that the Pico program is responding.
* `trace`: prints the most recent ROM addresses fetched by the Minitel's CPU.
