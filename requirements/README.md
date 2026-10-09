# Rev2 requirements

Started 2026-10-08. This directory is the working specification for a new design;
the existing board, CUPL, Xilinx and Raspberry Pi software are historical material.
Requirement IDs remain stable as the design develops. Proposed choices can change;
unresolved decisions belong in [open_issues.md](../open_issues.md).

| Document | Contents |
| --- | --- |
| [00_sources.md](00_sources.md) | Source provenance and what has actually been established |
| [01_system.md](01_system.md) | Scope, requirements and acceptance criteria |
| [02_hardware.md](02_hardware.md) | Block diagram, electrical design, preliminary connections |
| [03_interface.md](03_interface.md) | HP bus semantics and MCU/CPLD SPI contract |
| [04_firmware.md](04_firmware.md) | SD ownership, cassette engine, UI and board integration |
| [05_verification.md](05_verification.md) | Running the tests and remaining physical validation |
| [06_existing_connector.md](06_existing_connector.md) | Existing KiCad J1 pad/net assignments, sides and geometry |
| [07_tape_analysis.md](07_tape_analysis.md) | Server/Controller roles, per-file headers, supplied images and HP BOF versus TAP file marks |
| [08_pin_mapping.md](08_pin_mapping.md) | MCU/CPLD package pins, FT input constraints, debug/USB circuit and tentative placement |

The current implementation is a VHDL functional prototype with a three-sheet
KiCad draft and unrouted placement. The simplified CPLD fits with fixed pins and
generates JEDEC; RTL and fitter-emitted functional equations pass the handshake
bench. Timing, fitter-report accounting, electrical and PCB sign-off and STM32
target firmware remain open.
