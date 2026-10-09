# MPSI Rev2

A new HP 9830 interface based on
[Brent Hilpert's MPSI](https://madrona.ca/e/HP9830/mpsi/index.html): a 5 V ATF1504AS
CPLD handles the bus, and an STM32F103CB serves files from a microSD card.
Two 74HCT165s capture input, separate SPI buses serve the HP link and SD,
and an SSD1306 OLED with up/down/select buttons controls operation.

MPSI Server handles paper tape, printer capture and Fast Mode cassette emulation.
Tape Controller uses an HP-resident MTAPE proxy to read/write the physical drive.
USB MSC takes exclusive ownership of SD and releases every HP output.
See [the tape analysis](requirements/07_tape_analysis.md).

The current draft includes:

- [VHDL RTL, handshake bench and container build flow](hardware/rev2/README.md).
- [STM32/CPLD package pins and tentative placement](requirements/08_pin_mapping.md).
- [Three-sheet KiCad schematic and unrouted placement](hardware/rev2/kicad/README.md),
  with the existing HP connector, USB-C protection and PA9/PA10 debug header.
- [Portable C firmware core](firmware/rev2/README.md).

Start with [requirements](requirements/README.md) and [open issues](open_issues.md).

```sh
make test
```

Requires make, a C11 compiler and either GHDL or the documented Docker image.
Tests cover bus handshakes, five interrupt choices, GP-only Controller decoding,
direct configuration updates, unbuffered data, CS-clocked flags, input capture, tape commands
and exclusive SD/MSC ownership.

The simplified ATF1504 design fits and generates JEDEC with all 29 requested pins,
JTAG and fourteen open-collector HP outputs. RTL and fitter-emitted functional
equations each pass 2,000 checks at 9, 18 and 24 MHz SPI. These are zero-delay
tests; the F103 is specified up to 18 MHz, and 9 MHz is the provisional board rate.
This remains a design draft. Timing, fitter-report accounting, electrical current/rail and power-up validation, PCB routing,
and Arduino SD/USB/UI/Controller drivers also remain open. The STM32 pin map uses
PB14/PB10 as direct 5 V tolerant inputs, subject to supply/reset constraints.

Previous files remain in `MPSIBOARD`, `hardware/pld` and `software`.
The original README is preserved in [docs/legacy_design.md](docs/legacy_design.md).
