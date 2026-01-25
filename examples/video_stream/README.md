# Video Stream

<p align="center">
<img src="pictures/video.webp" width="30%" />
</p>

This Minitel program receives and immediately displays 80x75 monochromatic
images. The images are received through the serial port at a much higher baud
rate than the usual Minitel speeds (e.g. 115200 for the Minitel 2).

The images consist of a 40x25 grid of mosaic characters (G10 charset). Each
mosaic character contains 2x3 tiles, that can be individually set to white or
black.

The companion [send-video-frames.py](scripts/send-video-frames.py) script:
1. takes an animated image in input (e.g. a GIF)
2. dithers and turns its frames into mosaic characters
3. lastly, streams them to the Minitel in a loop.

## Maximum baud rate

The following table shows the fastest baud rates that were attained in each
tested Minitel model. Try lower speeds if experiencing data corruption.

| Model                    | Baud rate |
|--------------------------|-----------|
| RTIC Minitel 1           | 57600     |
| Philips Minitel 2        | 115200    |
| Telic-Alcatel Minitel 12 | 19200     |

## Quick start

Connect the Minitel's serial port to the computer and launch the ROM. Then, run
the following command on the computer:
```shell
# Replace 115200 with the actual baud rate.
$ python3 scripts/send-video-frames.py \
    --serial-port /dev/ttyUSB0 \
    --baud-rate 115200 \
    steamboat.gif
```
