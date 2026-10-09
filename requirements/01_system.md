# System requirements

| ID | Requirement | Acceptance |
| --- | --- | --- |
| SYS-001 | Start a separate Rev2 design using a 5 V ATF1502AS or ATF1504AS CPLD and an STM32F103 MCU. | Existing design remains available; Rev2 has its own source and tests. |
| SYS-002 | Provide independently programmable four-bit GP and TP select codes, optional SC15 printer service, and a three-bit selector for nDI0, nDI7, nSI0, nSI1 or nSI2. | Test all supported selections and reserved-code behaviour, then validate on HP. |
| SYS-003 | Support General/standard Mode stream input/output and Fast Mode 9865 cassette operations. | Protocol simulation, followed by PTAPE/LIST/LOAD/STORE/TLIST hardware trials. |
| SYS-004 | Use two cascaded 74HCT165s to capture sixteen HP output bits and read them through SPI. | One finite capture pulse per request; no loading while an unread request is shifted. |
| SYS-005 | Shift outgoing data/status into the CPLD and gate them onto the HP bus. | Bus sees valid data before ready flags; all driven pins are low or released. |
| SYS-006 | Supply selected SD files to the HP and save incoming data to SD. | Real SD file round trips, checksums and error handling; portable byte/tape core first. |
| SYS-007 | Offer USB MSC access to the entire SD card while HP file service is disabled. | Exclusive card ownership and flush/unmount/remount sequence tested. |
| SYS-008 | Use a 128x64 SSD1306-compatible I2C OLED and three buttons: up, down, select. | File selection, settings, service state and MSC handover work without a host terminal. |
| SYS-009 | Keep requirements here and issues in a root document with exactly four columns. | Stable issue numbers; status is `open` or `closed`; response records the resolution. |
| SYS-010 | Provide synthesizable VHDL and a reproducible test bench based on the referenced HP documentation. Use the user's GHDL/Yosys/Atmel-fitter container to produce JEDEC. | GHDL simulation passes; ATF1504 fitting, pin assignment, timing report and JEDEC generation follow. |
| SYS-011 | Provide mutually exclusive MPSI Server, Tape Controller and USB MSC roles. Server supplies/captures SD files; Controller uses an HP-resident proxy to read/write the physical cassette; MSC releases every HP output and disables both HP roles. | Mode/dispatcher tests, physical proxy/cassette trials and exclusive SD ownership checks. |

Prototype choices: ATF1504AS in a 44-pin package and STM32F103CBT6 in LQFP48.
The simplified VHDL RTL has 36 register bits, exceeding an
ATF1502's 32-register capacity.
An ATF1502 version requires a different register architecture or more external
logic. The corrected fitter now declares a fit and emits JEDEC with all requested
pins retained. RTL and fitter-emitted functional equations pass the handshake
bench. Timing and the fitter's inconsistent resource totals remain in 005/028/029.

Default proposal: GP=8, TP=9, printer disabled until explicitly enabled, interrupt
selector=3 (nSI1/DI9 under the revised mapping). The VHDL bench exercises this mapping and the printer-enabled configuration.
Zero disables a decoder without responding to SC0; firmware rejects SC10,
duplicate nonzero GP/TP addresses and overlap with enabled SC15. Other attached devices can occupy additional codes; these
must be checked by the user when configuring the actual installation.

First delivery: documented architecture, executable bus RTL/test bench, and a
portable C core. A three-sheet schematic and unrouted placement now exist; target drivers,
OLED implementation, SD/SdFat,
USB stack, image import, the Tape Controller/proxy frontend and validation on
an HP are subsequent milestones. The current core implements server functions;
its SERVING storage state does not distinguish the two required HP roles yet.

The two non-MSC roles are different applications over the same interface.
Controller mode keeps GP available for directives/replies, disables TP/printer
emulation, and leaves the HP proxy to access the real internal drive at SC10.
MSC leaves the CPLD powered/clocked but logically disconnects it from the HP by
releasing all outputs and disabling HP handlers. See [the tape analysis](07_tape_analysis.md).

Accepted user decisions, 2026-10-08: DI/SI/CFI/SSI pull-ups are 1 kOhm; the operator is
responsible for avoiding MSC during HP use; HP bus power and USB VBUS each feed
the board through a Schottky diode; an SD card-detect input is required; recovery
from protocol failure uses an operator restart rather than additional recovery
protocols, with USB unplugged during the full power cycle. Generate 24 MHz on
PA8 TIM1_CH1; timing validation remains open. Use Arduino_Core_STM32 with SdFat,
reuse the user's GHDL/Yosys/Atmel-fitter flow, store name/value configuration on
FAT and automatically rescan an inserted SD card. Use SIMH TAP as the cassette
container. HP BOF starts a file; a TAP mark conventionally terminates/separates
files. The earlier raw-cell substitution round-trips the sample but does not
establish the production logical-file mapping. Finalize file/record grouping
and padding preservation in issue 009; target backend, physical operations and
additional-media validation remain open. In MSC keep
the CPLD clocked with RESET_n released and HP_RUN low.

Accepted user decision, 2026-10-09: use VHDL for Rev2 RTL and add the private
`MattisLind/atf15xx-yosys-docker` repository as a Git submodule at
`hardware/rev2/rtl/atf15xx-yosys-docker`. Use SSH for repository access. The
submodule is present at the corrected path, the VHDL port and GHDL bench pass,
and the Docker image builds. The simplified design now fits with retained JTAG
and open-collector HP outputs; issue 004 records the image-generation result.

Additional decisions, 2026-10-09: use powered-state FT STM32 inputs directly
for 5 V MISO/request, reserve PA9/PA10 USART1 on a four-pin debug header,
reuse the current HP connector symbol/footprint, place CPLD/HCT165s beside it,
and copy the paper-tape project's USB-C/protection circuit. The package map
and startup constraints are recorded in 08_pin_mapping.md; unresolved electrical
and physical validation is recorded in issues 026/027.

RTL simplification agreed 2026-10-09: trust STM32 hardware to shift sixteen bits;
remove bit counting/frame-valid checks, the response latch bank, separate COMMIT,
unread-word protection and overrun logic. Shift config with HP selection masked
by CFG_FRAME. Data/status come directly from the shift register. Retain Hilpert's
three flag flip-flops, using CS rising for the CTL end-of-transfer function.
Firmware schedules SPI around HP busy/ready handshaking. The proposed 24 MHz SPI
exceeds the F103's 18 MHz specification; start at 9 MHz pending timing work in 028.
