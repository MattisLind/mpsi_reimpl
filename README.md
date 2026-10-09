# MPSI Rev2

A new HP 9830 interface based on
[Brent Hilpert's MPSI](https://madrona.ca/e/HP9830/mpsi/index.html): a 5 V CPLD
handles the bus, and an STM32F103 serves files from a microSD card.

The proposed hardware uses an ATF1504AS, two 74HCT165 input shift registers,
separate SPI buses for the HP link and SD card, USB MSC, and a 128x64 SSD1306 OLED
with up/down/select buttons. General/standard Mode handles paper tape and output
capture; Fast Mode handles cassette emulation. USB MSC takes exclusive ownership
of SD and disables HP service until the card is returned.

The second HP application, Tape Controller, uses an MTAPE proxy inside the HP to
read/write the real cassette drive through the General interface. It is mutually
exclusive with Server and MSC. See [the reader/writer and image analysis](requirements/07_tape_analysis.md).

Start with [the requirements](requirements/README.md) and
[open issues](open_issues.md). The first prototype includes
[Verilog and a test bench](hardware/rev2/README.md) and a
[portable C firmware core](firmware/rev2/README.md).

VHDL is now the selected RTL language, using the private `atf15xx-yosys-docker`
repository as the planned tool submodule under `hardware/rev2/rtl`. The VHDL port
is pending; [the hardware notes](hardware/rev2/README.md) document registration
and the current SSH authentication issue.

```sh
make test
```

Requires Icarus Verilog, make and a C11 compiler. Tests cover bus handshakes and
gating, capture/overrun, SPI framing, tape commands and exclusive SD/MSC ownership.

This is an initial design, not hardware ready for manufacture. Current/rail-voltage
and timing validation, CPLD fitting, a new schematic and Arduino SD/USB/UI drivers
remain. The draft has 59 registers, so ATF1504 is the initial target; ATF1502 needs
a different architecture. The checked ATF1504 datasheet guarantees DC VOL at
**12 mA**, so direct bus drive remains provisional.

The requirements now specify a 24 MHz PA8 TIM1 clock, the reference interrupt
gating and five ID lines, Arduino_Core_STM32 with SdFat, and SIMH TAP storage.
The prototype RTL and portable core still need these revisions, Controller role
support and target integration.

The previous files remain in `MPSIBOARD`, `hardware/pld` and `software`.
The original README is preserved in [docs/legacy_design.md](docs/legacy_design.md).
