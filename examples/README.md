# Example native programs for the Minitel

## Programs

* [`hello_world`](hello_world/):
  Shows a test pattern, the keyboard state and the time since boot.
* [`image_gallery`](image_gallery/):
  Displays arbitrary images by tiling them into a set of custom glyphs.
* [`video_stream`](video_stream/):
  Plays 80x75 monochromatic videos by receiving frames at a high baud rate over
  the p&eacute;ri-informatique port.

## Example workflow
```shell
$ cd hello_world

# Build for the RTIC Minitel 1 (NFZ 330).
$ mkdir build
$ cd build
$ cmake .. -DMINITEL_MODEL=nfz330
$ make  # the resulting ROM image will be in build/hello_world.bin
```
