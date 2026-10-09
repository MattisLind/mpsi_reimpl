# Sources and evidence

Reviewed 2026-10-08; package pins, USB circuit and VHDL tools updated 2026-10-09. No numerical HP bus timing limits are inferred from the test
bench. Simulation delays are engineering assumptions pending measurement.

| ID | Source | Use |
| --- | --- | --- |
| S01 | [Brent Hilpert: MPSI](https://madrona.ca/e/HP9830/mpsi/index.html#technical) | Device selection, serial registers, status/data gating and flag resets |
| S02 | [HP 9830 machine architecture](https://madrona.ca/e/HP9830/machine.html#sc) | General Mode (the requested standard mode) and Fast Mode transaction sequences |
| S03 | [MPSI schematic, revision 1.2](https://madrona.ca/e/HP9830/mpsi/mpsiSchematic.pdf) | Capture monostable, open-collector outputs, ACK correction and original connector |
| S04 | [HP 9830 hardware architecture](https://madrona.ca/e/HP9830/hardware.html) | 8 MHz MCK and physical tape/control-byte model |
| S05 | [HP 9830 redrawn schematic](https://madrona.ca/e/HP9830/hp9830Schematic.pdf) | F3, PDF page 19: DI/SI/CFI/SSI pull-ups; F2: register and CEO logic |
| S06 | [ATF1504AS(L) datasheet, 0950P, March 2014](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-0950-CPLD-ATF1504AS(L)-Datasheet.pdf) | Open-collector option, 64 macrocells, clock resources, DC and package constraints |
| S07 | [Microchip ATF1502AS](https://www.microchip.com/en-us/product/ATF1502AS) | 5 V, 32-macrocell alternative |
| S08 | [ST STM32F103x8/xB datasheet, DS5319](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf) | Rev 20, July 2025: LQFP48 pins, FT inputs, pull restrictions and supply-dependent voltage limits; PB14 SPI2 MISO is FT and is used for HP; PA6 is not FT and is used for 3.3 V SD |
| S09 | [Nexperia 74HC/HCT165, revision 8](https://assets.nexperia.com/documents/data-sheet/74HC_HCT165.pdf) | HCT thresholds, asynchronous parallel load and shift clock |
| S10 | [TI SN74LVC1G125](https://www.ti.com/product/SN74LVC1G125) | Earlier input-translator candidate, superseded by tentative direct FT inputs; retain as a fallback if issue 026 requires isolation |
| S11 | [Existing server](../software/mpsiServer/tp.c), [paper tape](../software/mpsiServer/ptr.c), [tape format](../software/tp9830/tpf.c) | Repository copy of Hilpert's published support software; cassette commands and empirical timing caveats |
| S12 | Tony Duell's HP 9830 schematic, as reported by the user on 2026-10-08; drawing/page reference not supplied | 1 kOhm pull-ups on DI, SI, CFI and SSI; accepted design input |
| S13 | [ST RM0008 reference manual](https://www.st.com/resource/en/reference_manual/cd00171190-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-arm-based-32Bit-mcus-stmicroelectronics.pdf) | Section 7.2.10 MCO choices and chapter 14 TIM1 PWM/counter configuration |
| S14 | [DECPROMEM VHDL build notes](https://github.com/MattisLind/DECPROMEM/blob/63c2cc10e1795c9c593e9b0065bb4efebdc5d622/vhdl/README.md); local atf15xx-yosys-docker checkout under /Users/mattis_lind/DECPROMEM/vhdl | Inline --PIN convention and GHDL/Yosys/Atmel-fitter flow; selected tool container now built/executed under S21 |
| S15 | [HP21xx firmware](https://github.com/MattisLind/hp21xx_papertape_emulator/blob/06787ada3dd411305c0451a057edc152249f1b82/src/hp21xx_papertape_emulator/hp21xx_papertape_emulator.ino), [README](https://github.com/MattisLind/hp21xx_papertape_emulator/blob/06787ada3dd411305c0451a057edc152249f1b82/README.md), [schematic](https://github.com/MattisLind/hp21xx_papertape_emulator/blob/06787ada3dd411305c0451a057edc152249f1b82/kicad/hp21xx_papertape_emulator/hp21xx_papertape_emulator.kicad_sch) | Arduino_Core_STM32/SdFat/TinyUSB/UI reference; exact USB-C symbol/footprint, USBLC6-2SC6, 22 Ohm/CC/pull-up/VBUS-divider circuit and diode/regulator source reused; reference firmware not built |
| S16 | [SIMH magtape representation](https://simh.trailing-edge.com/docs/simh_magtape.pdf), Bob Supnik, 2022-01-17 | TAP record lengths, padding, trailer, tape marks and standard/extended distinction |
| S17 | [Existing KiCad PCB](../MPSIBOARD/MPSIBOARD.kicad_pcb), [current schematic](../MPSIBOARD/MPSIBOARD.kicad_sch) | J1 embedded pad/net assignments, sides, pitch and outline; extracted in 06_existing_connector.md |
| S18 | [Hilpert's XNS format](https://madrona.ca/e/xns.html), [local image decoder](../software/util/util.c), [local tape server](../software/mpsiServer/tp.c) | Confirm that the supplied .t98/server path uses XNS and preserves control-marked cells |
| S19 | [Published support archive](https://madrona.ca/e/HP9830/pgm9830.tgz), downloaded 2026-10-08; digest in 07_tape_analysis.md | Contains one .t98 and two substantive .f98 images; all three and five inspected source/documentation files match the local copies |
| S20 | [Tape controller reader/writer](../software/tp9830/t98_mpsi.c), [format routines](../software/tp9830/tpf.c), [format offsets](../software/tp9830/tpf.h), [MTAPE proxy](../software/machine/mbb.s), [user documentation](../software/tp9830/helpdoc.txt) | Physical capture loses BOF control flags; buffered writer segments, GP directives, checksums and EOT-placeholder convention |
| S21 | [User's private atf15xx-yosys-docker repository](https://github.com/MattisLind/atf15xx-yosys-docker), selected 2026-10-09 | SSH submodule at the corrected path, pinned to 95e46e5f11254cc865614a9dc12fddb522a32f55. Built container synthesizes/fits the simplified 36-register design, retains 29 pins/JTAG/open-collector outputs and generates JEDEC. RTL and fitter-emitted functional equations pass 2,000 checks per SPI rate at 9/18/24 MHz; timing/fuse validation and report accounting remain open |

S05 is a reverse-engineered drawing, not HP's manufacturer schematic. Its F3
sheet shows fourteen pull-ups for twelve input bits plus CFI and SSI, but the
resistance text is just `K`. The user's S12 information establishes 1 kOhm for
all DI/SI/CFI/SSI lines; use that value for the design. Full receiver/capacitive
loads remain electrical verification work in issue 002. The unrelated 2.2 kOhm
resistors are not the evidence.

S06's DC table (PDF page 11) specifies VOL <= 0.45 V at IOL = 12 mA. The user's
24 mA assumption is therefore unverified for this part. Neither an absolute
maximum nor an AC drive-current plot establishes a guaranteed DC sink rating.
Recheck the current datasheet of the exact ordered variant before sign-off.

S01 and S03 inform the bus logic; S02 supplies transaction-level stimulus. S11
supports the initial tape-command core, including commands 6/7 as continuation.
The unusual read-forward delay in that implementation is an empirical observation,
not a bus specification and not yet implemented in the new firmware.

The user identified five interrupt choices in S03: nDI0, nDI7, nSI0, nSI1 and nSI2.
This supersedes the first draft's provisional eight-line lookup. VHDL and the portable configuration helper now implement those five lines;
the bench verifies reserved-code silence and reference status/ID combination.
Review of S01/S03 also resolves issue 015: preserve the reference's normal bus
gating and add the interrupt ID sink; do not globally replace status/data with
only the ID bit as the first Rev2 prototype did.

S04 explicitly names BOF as Beginning-of-File control-0x3C; S20 and the supplied
image show its XNS value 0x083C and the per-file 12-word header. The initial
BOF-to-TAP-mark experiment, documented in [07_tape_analysis.md](07_tape_analysis.md),
preserves all cells but changes conventional logical-file boundaries by treating
initial padding as a file. The user's correction distinguishes HP BOF from TAP
file-terminating marks; the production mapping is reopened in issue 009.
Experimental cell round trips and BOF recovery are verified on the one supplied
cassette image, not on additional tapes or hardware. The user's clarified scope
also requires a Tape Controller application, absent from the current core.

The built S21 image is sha256:ae26dbf975c8b88f82b73c99cd7dbde4705eb6e9bfc52ec7784e4114d821d461.
Its synthesis tools report Yosys 0.55+144 (6440499e5), GHDL LLVM
6.0.0-dev (5.1.1.r30.g69b0c75d7.dirty), Atmel fit1504 version 1918 (2007-03-21),
and atf15xx_yosys commit bd547977661bc7de08872df61108b229c6d78976.
The image also contains a separately built GCC-backend GHDL 7.0.0-dev;
the wrapper PATH selects the OSS CAD LLVM version. The pinned Dockerfile still
fetches moving GHDL/tool sources, so pinning the submodule alone does not pin all
build inputs; record the image digest/versions when comparing fitter iterations.
The local synthesis wrapper aligns the liberty TRI port to aprim.lib's ENA,
leaving the submodule untouched. The uncorrected EN/ENA flow discarded useful
logic and produced a misleading apparent fit; that image was rejected/removed.

The current simplified design follows Hilpert's direct MSR data/status path and
separate CTL-clocked ACK/CFI/SSI flip-flops; CS rising replaces CTL's flag-loading
edge. No overrun monitor or output data buffer is added. The reference text and
local mpsi.c also show CTL low inhibiting input reload during shifting, and note
prompt response for unacknowledged Fast commands. The finite CPLD capture pulse
remains a replacement for the reference's monostable.

DS5319 Table 43 specifies 18 MHz maximum SPI frequency. Nexperia S09 specifies
HCT165 CP-to-Q7 delay, DS setup and MISO sampling timing that must be budgeted
separately from raw word duration. The 24 MHz proposal is tested only as ideal
functional stimulus; it is not a supported F103 hardware setting.
