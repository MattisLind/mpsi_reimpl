# Rev2 portable firmware core

```sh
make -C firmware/rev2 test
```

`mpsi_protocol.h` defines the SPI payload and captured request layout.
`mpsi_storage.c` implements FAT/MSC ownership and General Mode file-byte operations.
`mpsi_tape.c` implements the initial cassette-command engine against a bounded
random-access cell cache interface.

These are server functions. The distinct Tape Controller application, using the
HP-resident MTAPE proxy for physical cassette access, is specified in
[the tape analysis](../../requirements/07_tape_analysis.md) and is not implemented yet.

See [the integration contract](../../requirements/04_firmware.md). Callbacks have
a host-test implementation only. There is no flashable Arduino STM32 image,
SdFat driver, USB stack, input queue, OLED driver, TAP backend or image converter
in this delivery.
Supply every required callback before init and serialize calls as specified.
A FAULT needs explicit board recovery; do not reinitialize while USB owns the card.
