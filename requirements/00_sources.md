# Sources and evidence

Reviewed 2026-10-08. No numerical HP bus timing limits are inferred from the test
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
| S08 | [ST STM32F103x8/xB datasheet, DS5319](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf) | USB, SPI, I2C, RAM, pin functions and voltage limits |
| S09 | [Nexperia 74HC/HCT165, revision 8](https://assets.nexperia.com/documents/data-sheet/74HC_HCT165.pdf) | HCT thresholds, asynchronous parallel load and shift clock |
| S10 | [TI SN74LVC1G125](https://www.ti.com/product/SN74LVC1G125) | Candidate 5 V input / 3.3 V output translator; exact power-off behaviour must be checked for the ordered part |
| S11 | [Existing server](../software/mpsiServer/tp.c), [paper tape](../software/mpsiServer/ptr.c), [tape format](../software/tp9830/tpf.c) | Repository copy of Hilpert's published support software; cassette commands and empirical timing caveats |
| S12 | Tony Duell's HP 9830 schematic, as reported by the user on 2026-10-08; drawing/page reference not supplied | 1 kOhm pull-ups on DI, SI, CFI and SSI; accepted design input |
| S13 | [ST RM0008 reference manual](https://www.st.com/resource/en/reference_manual/cd00171190-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-arm-based-32Bit-mcus-stmicroelectronics.pdf) | Section 7.2.10 MCO choices and chapter 14 TIM1 PWM/counter configuration |
| S14 | [DECPROMEM VHDL build notes](https://github.com/MattisLind/DECPROMEM/blob/63c2cc10e1795c9c593e9b0065bb4efebdc5d622/vhdl/README.md); local atf15xx-yosys-docker checkout under /Users/mattis_lind/DECPROMEM/vhdl | User's GHDL/Yosys/Atmel-fitter flow; local wrappers and fit1504.exe inspected, not executed |
| S15 | [HP21xx firmware](https://github.com/MattisLind/hp21xx_papertape_emulator/blob/06787ada3dd411305c0451a057edc152249f1b82/src/hp21xx_papertape_emulator/hp21xx_papertape_emulator.ino), [README](https://github.com/MattisLind/hp21xx_papertape_emulator/blob/06787ada3dd411305c0451a057edc152249f1b82/README.md), [schematic](https://github.com/MattisLind/hp21xx_papertape_emulator/blob/06787ada3dd411305c0451a057edc152249f1b82/kicad/hp21xx_papertape_emulator/hp21xx_papertape_emulator.kicad_sch) | Arduino_Core_STM32, SdFat, TinyUSB, three-button UI and diode-source reference; source reviewed, not built |
| S16 | [SIMH magtape representation](https://simh.trailing-edge.com/docs/simh_magtape.pdf), Bob Supnik, 2022-01-17 | TAP record lengths, padding, trailer, tape marks and standard/extended distinction |
| S17 | [Existing KiCad PCB](../MPSIBOARD/MPSIBOARD.kicad_pcb), [current schematic](../MPSIBOARD/MPSIBOARD.kicad_sch) | J1 embedded pad/net assignments, sides, pitch and outline; extracted in 06_existing_connector.md |
| S18 | [Hilpert's XNS format](https://madrona.ca/e/xns.html), [local image decoder](../software/util/util.c), [local tape server](../software/mpsiServer/tp.c) | Confirm that the supplied .t98/server path uses XNS and preserves control-marked cells |
| S19 | [Published support archive](https://madrona.ca/e/HP9830/pgm9830.tgz), downloaded 2026-10-08; digest in 07_tape_analysis.md | Contains one .t98 and two substantive .f98 images; all three and five inspected source/documentation files match the local copies |
| S20 | [Tape controller reader/writer](../software/tp9830/t98_mpsi.c), [format routines](../software/tp9830/tpf.c), [format offsets](../software/tp9830/tpf.h), [MTAPE proxy](../software/machine/mbb.s), [user documentation](../software/tp9830/helpdoc.txt) | Physical capture loses BOF control flags; buffered writer segments, GP directives, checksums and EOT-placeholder convention |
| S21 | [User's private atf15xx-yosys-docker repository](https://github.com/MattisLind/atf15xx-yosys-docker), selected 2026-10-09 | Requested VHDL-to-JEDEC tool submodule at hardware/rev2/rtl/atf15xx-yosys-docker; registration awaits working SSH authentication, so its contents/commit have not been inspected |

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
This supersedes the first draft's provisional eight-line lookup. The requirements
now specify those five lines; RTL/firmware changes remain to be implemented.
Review of S01/S03 also resolves issue 015: preserve the reference's normal bus
gating and add the interrupt ID sink; do not globally replace status/data with
only the ID bit as the first Rev2 prototype does.

S04 explicitly names BOF as Beginning-of-File control-0x3C; S20 and the supplied
image show its XNS value 0x083C and the per-file 12-word header. The initial
BOF-to-TAP-mark experiment, documented in [07_tape_analysis.md](07_tape_analysis.md),
preserves all cells but changes conventional logical-file boundaries by treating
initial padding as a file. The user's correction distinguishes HP BOF from TAP
file-terminating marks; the production mapping is reopened in issue 009.
Experimental cell round trips and BOF recovery are verified on the one supplied
cassette image, not on additional tapes or hardware. The user's clarified scope
also requires a Tape Controller application, absent from the current core.
