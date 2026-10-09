# Verification

Run from the repository root:

```sh
make test
```

Requires a C11 compiler, make, and Icarus Verilog (`iverilog` and `vvp`).
Build products go in the ignored `build/rev2` directory.

First run, 2026-10-08: `make test` passes 200 HDL assertions and the portable C
suite. The standalone RTL also compiles in Verilog-2005 mode. Document links and
the issue table were checked; the archived README matches the original exactly.

The subsequent issue reviews accepted MCU-generated 24 MHz, five interrupt ID
lines with three reserved selector codes, reference interrupt gating, Arduino/
SdFat target integration and SIMH TAP storage. These are documentation changes:
the RTL/tests still use the earlier 8 MHz clock assumption, DI4..11 selector
mapping and extra interrupt masking; firmware has no target or TAP backend.
Update and verify them through issues 004, 005, 009 and 010 before claiming
coverage of the revised requirements.

Subsequent tape analysis added the explicit Tape Controller role and verified the
experimental raw-cell BOF/TAP encoding on the supplied machine.t98 image. The
user's correction distinguishes BOF from conventional TAP file termination;
logical-file mapping and exact padding preservation remain open in issue 009.
This analysis does not add a target TAP backend, proxy controller or per-role
CPLD enables.

On 2026-10-09 the user selected VHDL and the private atf15xx-yosys-docker tool
repository as a submodule under hardware/rev2/rtl. Registration currently fails
SSH authentication; the existing tests remain Icarus/Verilog until the RTL and
bench are ported. No VHDL simulation, container build or JEDEC is verified yet.

| ID | Check | Current result |
| --- | --- | --- |
| VER-001 | General output clears SI0, captures data, and restores ready. | RTL simulation passes. |
| VER-002 | General input direction, data before ACK, SIH gating and SC15 enable/disable. | RTL simulation passes. |
| VER-003 | Fast read/write held CEO, finite PL, no recapture, CFI and CEO feedback. | RTL simulation passes. |
| VER-004 | All select codes; five ID lines and three reserved codes; reference normal-status/ID combination, SIH inhibit and CEO clear. | Earlier mapping and blanket interrupt masking pass simulation; revised mapping, reserved codes and reference gating remain untested. |
| VER-005 | Exact SPI length, truncated/overlong frames, config interlocks and safe unconfigured state. | RTL simulation passes. |
| VER-006 | Unread capture preserved, unread/busy overrun detected, reset/MSC releases bus. | RTL simulation passes. |
| VER-007 | File bytes/EOF, cassette read/reverse/continue/control marks, write protection and bounded cache retries. | Host C tests pass with fake media. |
| VER-008 | FAT/MSC exclusion, handover ordering, LBA bounds, host-eject interlock and injected failures. | Host C tests pass with fake filesystem/USB. |
| VER-009 | CPLD fitting, open-collector output mapping, clocks, reset recovery and post-fit timing. | Not run; required before PCB sign-off. |
| VER-010 | Arduino STM32 cross-build, SdFat/MSC round trips, automatic card rescan/media changes, buttons/OLED and measured service latency. | Target integration not implemented. |
| VER-011 | Validate fourteen 1 kOhm loads and diode-fed rail; measure CEO width, CO/SO/DO stability, PL and setup to ready flags at 24 MHz. | Pull-up values supplied by user; electrical/timing measurements not performed. |
| VER-012 | HP PTAPE/LIST/WRITE/LOAD/STORE/TLIST/rewind, physical MTAPE proxy read/write and role gating. | Not tested on HP; controller/role-enable implementation absent. |
| VER-013 | Analyze supplied XNS headers/checksums, compare .f98 contents, verify experimental TAP cell encoding and BOF recovery. | scripts/analyze_t98.py passes on the one supplied .t98 and two .f98 files. Unchanged legacy util.c/tpf.c independently rebuild all 4,895 cells exactly. The raw-cell experiment does not verify conventional TAP-file semantics; production mapping/backend remain open. |

The HDL bench cites the source pages in its header and drives the documented
General pulse/poll/SI0 and Fast held-CEO/CFI/feedback sequences. It also checks the
MPSI status/data gating policy. A simulation-only transparent-load/shift model
represents the two HCT165s, using non-inverting Q7.

Functional simulation does not model analog loading, metastability, propagation
skew, current limits, SD stalls or the HP CPU/ROM. Passing tests do not establish
hardware compatibility or ATF1504 fit. The bench's 2 us CEO pulse is a test choice,
not an HP minimum timing specification.

Next steps:

1. Validate bus current, diode-fed rail, existing connector geometry and capture timing.
2. Register the chosen tool submodule, port RTL/bench to VHDL, assign package pins
   and prove fit through the GHDL/Yosys/Atmel-fitter flow.
3. Draw a new schematic with voltage conversion and USB power isolation.
4. Implement Arduino STM32 SPI/link queue, SdFat cache, TinyUSB and SSD1306/button drivers.
5. Validate both bus modes and file persistence with captures from a real HP.
6. Implement/validate Controller GP jobs, per-role device enables and physical
   read streaming/buffered writes with the MTAPE proxy and its RAM limits.
