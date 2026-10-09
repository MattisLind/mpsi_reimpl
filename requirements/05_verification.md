# Verification

Run from the repository root:

```sh
make test
```

Requires make, a C11 compiler and either local GHDL or the documented Docker
image. Build products go in the ignored `build/rev2` directory. The smaller
simulation image can be built with `python3 scripts/rev2_hdl.py build-sim-image`;
see [hardware build notes](../hardware/rev2/README.md).

Verified 2026-10-09: the VHDL-2008 bench passes **2,000 checks at each of 9, 18
and 24 MHz SPI**, with a 24 MHz one-third-duty-cycle MCK. The portable C suite passes with the revised IRQ3 and
zero-address decoder-disable configuration. The full private tool container
builds; GHDL/Yosys synthesis passes its structural check. Corrected Atmel fitting
declares **a fit**, generates JEDEC and retains all 29 requested pins, fourteen
open-collector HP outputs, JTAG and disabled pin keepers. The simplified RTL and
fitter both retain 36 registers. A functional translation of the fitter-emitted
VHO gates/flip-flops passes the same 2,000 checks at each rate. Run it with:

```sh
python3 scripts/rev2_hdl.py postfit
```

This is a zero-delay equation check; SDF is emitted but not applied, JEDEC fuse
encoding is not independently decoded and hardware timing remains unverified.
The report's inconsistent 65/64 macrocell total is recorded in issue 029; do not
derive free-resource margins from it. Earlier apparent fitting with a disconnected
TRI enable was rejected; the build-local liberty correction preserves that logic.
The F103 is specified up to 18 MHz SPI. The 24 MHz run is functional stimulus;
9 MHz is the provisional board rate pending the MCU/HCT165/CPLD timing budget.

The three-sheet KiCad schematic passes ERC with zero violations. Exported nets
are checked against the MCU/CPLD package map, connector/debug pins, HCT165 word
order, SD detection and USB protection. The unrouted PCB has zero schematic
parity mismatches; fifty connector pad positions/sizes/angles/layers are checked
against the legacy board during generation. Placement has no shorts or courtyard
overlaps or silkscreen collisions. Final placement DRC reports two clearance
violations, 236 unrouted connections and zero schematic parity mismatches.
Manufacturing DRC is not complete: unrouted nets and the reference
USB-C footprint's 0.175 mm pad clearance need a production rule/geometry review.
The two clearance violations both concern that copied USB-C footprint.

| ID | Check | Current result |
| --- | --- | --- |
| VER-001 | General output clears SI0, captures data, and restores ready. | VHDL simulation passes. |
| VER-002 | General input direction, data before ACK, SIH gating and SC15 enable/disable. | VHDL simulation passes. |
| VER-003 | Fast read/write held CEO, finite PL, no recapture, CFI and CEO feedback. | VHDL simulation passes. |
| VER-004 | All select codes; five ID lines and three reserved codes; reference normal-status/ID combination, SIH inhibit and CEO clear. | Revised mapping, reserved-code silence and selected GP status beside ID pass VHDL simulation. |
| VER-005 | Sixteen-bit transfers, direct configuration masked during shifting, direct data/status and flags held until CS rises. | RTL and fitter-equation simulations pass at 9/18/24 MHz, including all CO values during a live config update. Firmware rejects invalid configurations. No hardware frame checking, output latch bank or separate COMMIT. |
| VER-006 | Finite input capture, one capture per CEO assertion, CS clears request/inhibits loads, reset/MSC/configuration releases bus. | RTL and fitter-equation simulations pass. A new request replaces an unread capture; no overrun monitor or unread-word protection is required. |
| VER-007 | File bytes/EOF, cassette read/reverse/continue/control marks, write protection and bounded cache retries. | Host C tests pass with fake media. |
| VER-008 | FAT/MSC exclusion, handover ordering, LBA bounds, host-eject interlock and injected failures. | Host C tests pass with fake filesystem/USB. |
| VER-009 | CPLD fitting, open-collector mapping, fixed pins, fitted equations, clocks and timing. | Synthesis/fitting, 29 retained pins, 14 open-collector outputs, JTAG/keepers, 36 registers and fitted-equation bench pass; JEDEC/VHO/SDF exist. Timing/fuse validation and report accounting remain in 005/028/029. |
| VER-010 | Arduino STM32 cross-build, SdFat/MSC round trips, media changes, buttons/OLED and measured service latency. | Target integration not implemented. Package contract now assigns HP to SPI2 and 3.3 V SD to SPI1. |
| VER-011 | Validate fourteen 1 kOhm loads and diode-fed rail; measure CEO width, CO/SO/DO stability, PL and setup to ready flags. | Pull-ups supplied by user; electrical/timing measurements not performed. Direct FT input supply/reset validation remains in 026. |
| VER-012 | HP PTAPE/LIST/WRITE/LOAD/STORE/TLIST/rewind and physical MTAPE proxy. | GP-only Controller decoder configuration passes simulation; firmware jobs and actual HP/cassette trials remain open. |
| VER-013 | Analyze XNS headers/checksums, compare .f98 contents, verify experimental TAP cell encoding and BOF recovery. | Earlier analysis passes on one .t98 and two .f98 files; legacy util.c/tpf.c independently rebuild all 4,895 cells exactly. Production conventional-TAP mapping/backend remain open. |
| VER-014 | Package/inline/netlist consistency, all MCU/CPLD/debug pins, HCT165 bit order, USB/SD wiring and schematic ERC. | Revised 29-pin assignment fits and matches netlist; all 348 PCB pad nets match; ERC zero. Physical sign-off remains open. |
| VER-015 | Reused connector geometry, placement shorts/courtyards and schematic-to-PCB parity. | Geometry assertion passes; no placement shorts/courtyard overlap; parity zero. Board is unrouted; remaining DRC/fabrication work is in 027. |

The HDL bench cites Hilpert's source pages in its header and drives documented
General pulse/poll/SI0 and Fast held-CEO/CFI/feedback sequences. A transparent-load/
shift model represents the two HCT165s using non-inverting Q7. It checks two
24 MHz load cycles and a request only after PL closes. Its 2 us CEO pulse is a
test choice, not an HP minimum specification. The capture controller's reset-release
recovery must still be measured or checked against valid fitted timing.

Functional simulation does not model analog loading, metastability, propagation
skew, current limits, SD stalls or the HP CPU/ROM. Neither ERC nor a simulation
pass establishes voltage compatibility, rail margin, physical timing or correctness
of the programmed device. The generated image is a candidate for hardware validation.

Remaining work: reconcile fitter resource accounting and verify the image/device;
validate capture/SPI timing; resolve bus loading,
diodes and direct-input startup; finish mechanical review/routing; implement
Arduino/SdFat/TinyUSB/SSD1306 drivers and Controller jobs; then exercise both
modes, cassette operations and file persistence on an actual HP.
