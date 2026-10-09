# Rev2 CPLD prototype

The selected RTL language is **VHDL** (user decision, 2026-10-09), built with
the user's GHDL/Yosys/Atmel-fitter container. The files below are the existing
Verilog prototype; the VHDL port and GHDL bench are still to be implemented.

```sh
make -C hardware/rev2 test
```

[rtl/mpsi_cpld.v](rtl/mpsi_cpld.v) contains bus logic and SPI configuration/output.
[tb/tb_mpsi.sv](tb/tb_mpsi.sv) supplies General/Fast Mode and fault-path stimulus;
[tb/hct165_pair.v](tb/hct165_pair.v) models the external input registers.

Read [the interface contract](../../requirements/03_interface.md) before writing
a driver. COMMIT is a separate input edge while CS is low. Firmware stages data
before setting ready flags and queues every qualified received request.

No physical pins, fitter result or programming file are supplied yet. ATF1504AS
is the first fitting target. This draft has 59 registers and 31 logical signals;
ATF1502 cannot hold this version's register state.

Add the private build-tool repository from the root of this project using SSH:

```sh
git submodule add git@github.com:MattisLind/atf15xx-yosys-docker.git hardware/rev2/rtl/atf15xx-yosys-docker
```

This registers the path in `.gitmodules` and stages that file plus the submodule
commit reference. Commit both together when ready. Each parent-repository commit
then pins one tool-repository commit, rather than automatically following its
latest branch. The registration attempt here failed SSH authentication; the
submodule has not yet been added. Run the command in a terminal with a GitHub
SSH key that has access to the private repository (load it with `ssh-add` first
if needed).

After registration, initialize the pinned tools in another checkout with:

```sh
git submodule update --init --recursive
```

Adding the submodule does not build the Docker image or generate JEDEC. Those
steps follow after inspecting the pinned scripts and implementing the VHDL.
