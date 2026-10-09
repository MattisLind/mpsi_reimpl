# KiCad draft

Open [MPSI_REV2.kicad_pro](MPSI_REV2.kicad_pro) in KiCad 10.

The root sheet contains the copied HP connector, ATF1504AS-10JU44, two 74HCT165s,
JTAG and reset/control pulls. The MCU sheet contains STM32F103CBT6, microSD with
card detection, OLED/buttons, USART1 debug, SWD, boot jumper and 8 MHz crystal.
The USB/power sheet copies the paper-tape project's USB-C/USBLC6 circuit and
Schottky source isolation, with TLV75533 3.3 V regulation.

The 64-footprint board is an **unrouted placement draft**. Its HP connector and
Edge.Cuts use the current legacy board. All fifty connector pad positions,
sizes, angles and layers are checked against that board when regenerated.
The CPLD and HCT165s sit beside the HP edge connector; MCU is to their right,
with SD/UI/debug/USB towards the other edges. Mechanical fit and routing remain
to be reviewed. The simplified CPLD fits with the requested package pins, including
CS moved to GCLK3 pin 41. COMMIT/overrun wiring is removed; PB0/PB11 and CPLD
34/39/40 are spare. Electrical/timing validation and routing remain open.

The project embeds symbols and supplies portable `Rev2.kicad_sym` and
`Rev2.pretty` libraries. The connector library is extracted from
`MPSIBOARD/MPSIBOARD.kicad_pcb`; board-file pad angles are normalized to library
angles without changing pad geometry. USB-C symbol/footprint and circuit values
come from [the paper-tape project](https://github.com/MattisLind/hp21xx_papertape_emulator/tree/06787ada3dd411305c0451a057edc152249f1b82/kicad/hp21xx_papertape_emulator).
Original projects are untouched.

The schematic generator uses installed KiCad symbol libraries and local copies
of that reference schematic/PCB. Its `--reference`, `--reference-pcb` and
`--symbols` arguments allow overriding the defaults. The saved KiCad project
does not depend on those source files being installed.

```sh
python3 scripts/generate_rev2_schematic.py
kicad-cli sch export netlist --output /tmp/mpsi-rev2.net hardware/rev2/kicad/MPSI_REV2.kicad_sch
python3 scripts/check_rev2_pins.py --netlist /tmp/mpsi-rev2.net
```

For the placement generator use KiCad's bundled Python with its `pcbnew` module;
it consumes the above netlist, including explicit unconnected pin nets. On macOS:

```sh
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9 scripts/generate_rev2_placement.py
```

`components.json` records the schematic components/UUID paths used by that
generator; `pinmap.json` in the parent directory is the package-pin contract.
Generators overwrite these draft files, so update generator source alongside
manual schematic changes if regeneration is needed.
