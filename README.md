CCLoader-RPi
========

Burn CC254x firmware using a Raspberry Pi.

Build
-----
`gcc src/CCLoader.c -std=c99 -o CCLoader`

Usage
-----
```
Usage: CCLoader [OPTION...] [firmware.bin]
Flash firmware on a CC254x chip, like those used in HM-10 modules.

  -C, --DC=pin               Raspberry Pi pin connected to the DEBUG_CLOCK (DC)
                             pin on the CC254x chip
  -D, --DD=pin               Raspberry Pi pin connected to the DEBUG_DATA (DD)
                             pin on the CC254x chip
  -r, --retries=RETRIES      Number of time to retry flashing a block after it
                             fails verification (defaults to 5)
  -R, --RESET=pin            Raspberry Pi pin connected to the RESET_N pin on
                             the CC254x chip
  -v, --no-verify            Do not verify each block after it is flashed
  -?, --help                 Give this help list
      --usage                Give a short usage message

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.
```
