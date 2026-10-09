# HP bus and SPI interface

| ID | Requirement |
| --- | --- |
| BUS-001 | Decode CO[3:0] as the actual select-code value. GP and printer use General Mode; TP uses Fast Mode. |
| BUS-002 | In General Mode, SO3=1 means input to HP; SO3=0 means output from HP. CEO clears SI0/ACK while asserted; firmware restores ACK after completion. |
| BUS-003 | In Fast Mode, keep CEO asserted until data is ready and CFI acknowledges; release CFI when CEO deasserts. |
| BUS-004 | General status is visible when selected; General data is visible when selected and nSIH=0. Fast status is visible when selected and CEO inactive; Fast data is visible when selected and CEO active. |
| BUS-005 | An SSI flag drives nSSI and a selected identifying bit only when nSIH=1. Device enable clears SSI. |
| BUS-006 | Preserve ACK during TP operations and restore it on tape responses. Reset and HP_RUN=0 release all HP outputs. |
| BUS-008 | Restrict interrupt selection to nDI0, nDI7, nSI0, nSI1 and nSI2. Reserved selector codes assert neither SSI nor an identifying bit. |
| BUS-007 | Capture once per selected CEO assertion, freeze before interrupting the MCU, preserve an unread request and report overrun. |
| BUS-009 | Enable only the GP relay interface in Tape Controller mode; disable TP and printer emulation. In MSC release all HP outputs. Add per-role enables or equivalent decode gating; the prototype's global HP_RUN alone cannot leave GP enabled while disabling TP. |
| SPI-001 | Use MSB-first, 16-bit, full-duplex SPI mode 2 (CPOL=1, CPHA=0). HCT165 and CPLD shift on the trailing rising edge. |
| SPI-002 | Stage outgoing data with ACK/CFI/SSI clear, then repeat the same data with the required flags set. Newly asserted ready signals shall follow stable data. |
| SPI-003 | Change configuration only with HP_RUN=0; do not resume without a valid configuration and initialized response. |

BUS-002/003 follow [S02](https://madrona.ca/e/HP9830/machine.html#sc);
BUS-004/005/006 follow the circuit policy in
[S01](https://madrona.ca/e/HP9830/mpsi/index.html#technical).
Issue 015 is resolved by following the reference circuit. Normal status and data
retain their existing gating; the interrupt ID is an additional open-collector
sink, not a global replacement for normal output. In logical active-high form:

```text
enabled = selected AND NOT nCEO
status_gate = (general_selected) OR (tape_selected AND NOT enabled)
data_gate = (general_selected AND NOT nSIH) OR (tape_selected AND enabled)
id_active = SSI_flag AND nSIH AND valid_interrupt_selector
DI_SI_sinks = normally_gated_data_status OR selected_ID_mask
```

`general_selected` includes the GP and enabled SC15 printer selections. The
selected-ID mask is zero unless `id_active`. These equations apply while reset
is released, HP_RUN is high and configuration is valid; otherwise outputs release.
Device enable clears the SSI flag.
Thus ordinary General data is inhibited during ID presentation by nSIH itself,
but status can still be visible when a General device is selected. When no device
is selected, only the configured ID bit is presented. The first Rev2 RTL globally
masks normal DI/SI when SSI is active; that deviation must be removed and its
tests revised. No additional bus-gating choice is required from the user.

All values below are **logical** values. A logical 1 on HP input data/status means
pulling the corresponding `nDI`/`nSI` wire low. Captured CO/SO/DO use non-inverted
HCT165 Q7, so firmware must not apply the legacy driver's XOR inversion.

| Configuration bits (CFG_FRAME=1) | Meaning |
| --- | --- |
| 3:0 | GP select code |
| 7:4 | TP select code |
| 8 | SC15 printer enable |
| 11:9 | Interrupt selector |
| 15:12 | Zero, reserved |

| Response bits (CFG_FRAME=0) | Meaning |
| --- | --- |
| 7:0 | Data for DI0..7 |
| 8 | Unused; SI0 comes from buffered ACK |
| 11:9 | SI1..3 status |
| 12 | Unused |
| 13 | ACK request |
| 14 | CFI request |
| 15 | SSI request |

MISO's captured word is `{CO[3:0], SO[3:0], DO[7:0]}`. A separate active-low
REQUEST_n GPIO qualifies whether that word contains a request. There is no
embedded request bit. The user's schematic review establishes five interrupt
choices. Use this revised selector assignment:

| Selector | HP wire | DI bit |
| --- | --- | --- |
| 0 | nDI0 | 0 |
| 1 | nDI7 | 7 |
| 2 | nSI0 | 8 |
| 3 (default) | nSI1 | 9 |
| 4 | nSI2 | 10 |
| 5..7 | Reserved: no SSI or ID assertion | None |

This is the required mapping for the next RTL/firmware revision. The existing
implementation still has its initial DI4..11 lookup and default selector 5;
its configuration validator also accepts all eight values. Those changes are
implementation work under issues 004/010, separate from the resolved line selection.

The configuration table above describes the current prototype payload. Role
gating must extend it with independent GP/TP enables or equivalent controls;
allocate/update the reserved bits, reset defaults and validators through issue
025 before claiming Tape Controller support. The SC15 enable already exists.
Controller directives/replies use the GP General handshake and the full usable
12-bit word, not the virtual-tape Fast handler. The relay itself talks to internal
SC10; the CPLD must remain silent for that code. See [the protocol trace](07_tape_analysis.md).

Transaction sequence:

1. Wait for PL to be high and REQUEST_n to have settled; inspect pending before CS.
2. Set CFG_FRAME and take CS low. Do not begin during capture.
3. Exchange exactly sixteen bits. Keep MOSI stable through each rising SCK edge.
4. Stop SCK high. Wait at least 125 ns (prototype guard), then pulse COMMIT high
   for at least 125 ns with CS still low and CFG_FRAME stable.
5. Keep CS low for at least another 500 ns so the consume toggle crosses MCK,
   then raise CS. Keep COMMIT low except for its one commit pulse per transaction.
6. If REQUEST_n was active before a data transaction, queue its received word even
   when the transaction was initiated to send a response. Never silently discard it.

The separate COMMIT edge avoids resetting the length counter on the same edge
that samples it. Invalid lengths cannot alter configuration/response/flags.
However, the external HCT165 chain has already shifted on a malformed transaction;
the input request cannot be recovered by retrying that shift. Report the failure;
the operator power cycles the HP with USB unplugged, as agreed in 016/024.
Removing both supplies also resets the adapter. No CRC, readback, replay or
additional menu recovery is required for protocol faults.

Nominal 8 MHz MCK capture: two synchronizer stages plus event detection start a
250 ns low PL pulse; closing PL and asserting REQUEST_n take at most about
625 ns from CEO, excluding propagation and metastability. Require at least one
further MCK period before starting SPI after the request indication. The **entire**
CO/SO/DO word must remain stable until PL closes. These assumptions need bus
measurements; a pulse that begins after the HP has changed its data is incorrect.

For the accepted 24 MHz PA8 clock, the same two-cycle PL pulse is about 83 ns and
five-cycle nominal freeze bound about 208 ns. These are derived from the current
RTL, not measured timing guarantees. Recheck HCT165 load timing and HP data hold.

At 8 MHz SPI each word needs 2 us plus guards/software. Fast Mode service should
target a response in the tens of microseconds after the request unless a confirmed
tape command requires a deliberate delay. OLED refresh and SD transactions must
not block that path. A single input mailbox is a prototype limitation: requests
during SPI or while another request is pending set sticky `overrun` and are lost.
Clearing HP_RUN clears pending/overrun on MCK. A faster MCU alone cannot prove
that unacknowledged STOP/rewind operations will never be missed.
