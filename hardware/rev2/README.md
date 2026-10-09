# Rev2 CPLD and board draft

[rtl/mpsi_cpld.vhdl](rtl/mpsi_cpld.vhdl) is the authoritative VHDL-2008 RTL.
[tb/tb_mpsi.vhdl](tb/tb_mpsi.vhdl) exercises General/Fast Mode, interrupt gating,
Controller decoder selection, direct SPI updates, CS flag strobes, input capture and MSC release.
[tb/hct165_pair.vhdl](tb/hct165_pair.vhdl) models the two external input registers.
The bench passes 2,000 checks at each of 9, 18 and 24 MHz SPI, with a 24 MHz,
one-third-duty-cycle capture clock. Only 9 MHz is the provisional board setting;
24 MHz exceeds the STM32F103's specified SPI limit. See issue 028.

The [pin map](../../requirements/08_pin_mapping.md) describes both packages,
direct STM32 FT inputs, USART1 debug header and tentative placement.
[kicad/MPSI_REV2.kicad_pro](kicad/MPSI_REV2.kicad_pro) opens the three-sheet
schematic and unrouted board. The local libraries preserve the existing HP
connector and the paper-tape project's USB-C footprint.

Read [the interface contract](../../requirements/03_interface.md) before writing
a driver. Configuration shifts directly with CFG_FRAME high, which blanks HP
service; it needs no COMMIT. STM32 SPI supplies sixteen clocks, without hardware
length checking. Data/status drive the HP gates directly. CS low clears REQUEST_n
and inhibits HCT165 loading; CS rising loads ACK/CFI/SSI after the data settles.
There is no separate COMMIT, output latch bank, unread-word lock or overrun monitor.
Firmware updates only during the HP handshake's safe interval and queues every
qualified received request. Address zero
disables the corresponding decoder; Controller GP8/TP-off uses word 0x0608.

Run from the project root:

```sh
git submodule update --init --recursive
python3 scripts/rev2_hdl.py build-image
make test
python3 scripts/rev2_hdl.py fit
python3 scripts/rev2_hdl.py postfit
```

The private SSH tool submodule is now at the intended path
`hardware/rev2/rtl/atf15xx-yosys-docker`, pinned to
`95e46e5f11254cc865614a9dc12fddb522a32f55`.
Its previously nested location has been corrected with `git mv`; commit the
staged `.gitmodules` and gitlink move together with the desired project changes.
No commit has been made here. Another checkout needs SSH access to the private
repository.

If local GHDL is installed, `make test` uses it. Otherwise it uses Docker. A
smaller simulation-only image is available with
`python3 scripts/rev2_hdl.py build-sim-image`; this is used when the full image
has not been built. Docker commands run without interactive terminal flags.
The wrapper locates Docker Desktop on macOS when its CLI is absent from PATH.

The synthesis flow retains DECPROMEM's inline `--PIN:` constraints. Four JTAG
constraints are included because the fitter writes them back into its pin file.
`-preassign keep` prevents silent reassignment. All fourteen HP pins are configured
open collector, pin keepers are off, and JTAG remains on. A build-local liberty
copy corrects the upstream TRI enable-name mismatch (`EN` versus Atmel's `ENA`);
the tool submodule is unchanged. `check -assert` rejects broken synthesized nets.

**The simplified implementation fits ATF1504 and generates JEDEC.** It has 36
register bits: data shift 16, configuration 12, flags 3, request synchronization 3
and capture pulse control 2. All 29 logical pins are retained; CS uses global
clock pin 41, formerly COMMIT. PB0/PB11 and CPLD pins 34/39/40 are now spare.
ATF1502 has only
32 registers and cannot hold this version either.

Logs, EDIF, JEDEC and the fitter's VHO/SDF go to `build/rev2/vhdl/fit`; RTL waveforms
go to `build/rev2/vhdl/mpsi-{rate}.vcd`. Stale fitted outputs are removed before synthesis, and
a successful fit must pass the package-pin check before being accepted.
`postfit` translates the fitter-emitted gates/flip-flops into functional VHDL and
runs the same bench: all 2,000 checks pass at each rate. It does not substitute RTL,
apply the SDF, prove timing or independently decode JEDEC fuses. The translation
rejects unsupported primitive types and checks the register count.

The fitter declares a fit but reports inconsistent resource totals (65/64
macrocells and Block D 17/16); issue 029 records this for review. Functional
fitted-equation tests pass, but report/fuse verification and physical
timing/current/rail validation remain before hardware sign-off.

For fitter-strategy iterations, use `python3 scripts/rev2_hdl.py fit --fit-options='...'`.
Requested package pins are still checked before accepting an image.
