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
| BUS-007 | Generate one finite HCT165 load pulse per selected CEO assertion, then assert REQUEST_n. CS low clears REQUEST_n and inhibits input loading during SPI. Rely on prompt firmware service and HP handshaking; no unread-word protection or hardware overrun monitor. |
| BUS-009 | Enable only the GP relay interface in Tape Controller mode; disable TP and printer emulation. In MSC release all HP outputs. Use select code zero to disable GP or TP independently; it shall never decode HP SC0. Controller sets TP=0 and printer off. |
| SPI-001 | Use MSB-first, 16-bit, full-duplex SPI mode 2 (CPOL=1, CPHA=0). HCT165 and CPLD shift on the trailing rising edge. |
| SPI-002 | Drive data/status directly from the output shift register. Shift only when the HP handshake permits changing them. Load ACK/CFI/SSI flip-flops on CS rising after the final SPI edge and data-settling guard; do not add an output buffer or separate COMMIT. |
| SPI-003 | Shift configuration directly into its register, without COMMIT. CFG_FRAME=1 disables HP selection, outputs and capture throughout the update. Firmware normally holds HP_RUN=0 and validates configuration before resuming service. |
| SPI-004 | Trust STM32 hardware to exchange sixteen bits per transaction. Do not add CPLD bit counting, frame-valid state or frame-length rejection. The proposed 24 MHz SPI rate exceeds the F103's specified 18 MHz limit; establish a rate meeting MCU, HCT165 and CPLD timing in issue 028. |

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
is released, HP_RUN is high, CFG_FRAME is low and at least one decoder is enabled;
otherwise outputs release. Configuration validation belongs to firmware.
Device enable clears the SSI flag.
Thus ordinary General data is inhibited during ID presentation by nSIH itself,
but status can still be visible when a General device is selected. When no device
is selected, only the configured ID bit is presented. The VHDL RTL and bench implement these equations, including selected General
status alongside interrupt identification. No additional bus-gating choice is
required from the user.

All values below are **logical** values. A logical 1 on HP input data/status means
pulling the corresponding `nDI`/`nSI` wire low. Captured CO/SO/DO use non-inverted
HCT165 Q7, so firmware must not apply the legacy driver's XOR inversion.

| Configuration bits (CFG_FRAME=1) | Meaning |
| --- | --- |
| 3:0 | GP select code; zero disables GP |
| 7:4 | TP select code; zero disables TP |
| 8 | SC15 printer enable |
| 11:9 | Interrupt selector |
| 15:12 | Firmware sends zero; hardware ignores these bits |

| Response bits (CFG_FRAME=0) | Meaning |
| --- | --- |
| 7:0 | Data for DI0..7 |
| 8 | Unused; SI0 comes from the ACK flip-flop |
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

The VHDL implementation uses this mapping; the portable firmware validator
accepts only 0..4. Hardware also accepts reserved selector values to allow
simulation of their required silent behaviour.

Address zero disables a decoder without consuming extra configuration bits or
registers. Default Server GP8/TP9/printer-off/IRQ3 is 0x0698; printer-on is
0x0798. Controller GP8/TP-off/printer-off/IRQ3 is 0x0608. Firmware rejects duplicate
nonzero addresses, SC10 and address overlap with an enabled printer. The CPLD
does not repeat this validation. Firmware sends reserved bits 15:12 as zero;
the configuration shift register retains only the final twelve bits.
With all three decoders disabled the interface
is unconfigured/silent, including interrupt identification. The bench checks
GP-only Controller decoding, TP-only decoding, SC0 silence and SC10 silence.
This provides hardware role gating; the firmware job dispatcher remains open.
Controller directives/replies use the GP General handshake and the full usable
12-bit word, not the virtual-tape Fast handler. The relay itself talks to internal
SC10; the CPLD must remain silent for that code. See [the protocol trace](07_tape_analysis.md).

Transaction sequence:

1. For HP service, wait for a request and PL high. Require at least one MCK period
   after REQUEST_n before SPI. Save whether REQUEST_n is low **before** lowering CS.
   During General input, ACK must remain clear until data is complete; during Fast
   input, CFI must remain clear until data is complete. Once ready is asserted,
   leave the data/status register unchanged until the next request establishes
   a new safe update interval. Initialize/configure with HP service disabled.
2. Set CFG_FRAME and take CS low. This clears REQUEST_n and inhibits HCT165 loading.
   Do not begin during PL low. Read requests with flags clear while firmware
   prepares their response; do not perform speculative idle SPI reads.
3. Exchange exactly sixteen bits. Keep MOSI stable through each rising SCK edge.
4. Stop SCK high and wait at least 125 ns (prototype data-settling guard), then
   raise CS. With CFG_FRAME low, this edge loads the three flag flip-flops from
   the final word; data/status are already present at the gates. It is Hilpert's
   CTL end-of-transfer function, with no extra GPIO or data-latch bank.
5. Leave at least 250 ns before another transaction (prototype capture-reset
   recovery guard). For configuration keep CFG_FRAME high through CS rising,
   then lower it; keep HP_RUN low until initialization is complete.
6. If REQUEST_n was active before a data transaction, queue its received word even
   when the transaction was initiated to send a response. Never silently discard it.

Data/status may change throughout SPI; the HP handshake determines when the
processor may use them. Only the three readiness/interrupt flags are separately
latched, so intermediate shift bits cannot assert ACK, CFI or SSI. One completed
response transfer supplies both data and flags; an extra data-first transfer is
not required. This follows the reference's unbuffered MSR and three CTL-clocked
flag flip-flops. Configuration needs no output latch because CFG_FRAME masks HP
service while its bits shift. There is no hardware bit counter or frame-valid check: STM32
SPI is responsible for sixteen clocks. A malformed transfer can corrupt a response
or configuration, and the shifted HCT165 input cannot be recovered by retrying.
If firmware detects a protocol failure, report it;
the operator power cycles the HP with USB unplugged, as agreed in 016/024.
Removing both supplies also resets the adapter. No CRC, readback, replay or
additional menu recovery is required for protocol faults.

The accepted 24 MHz PA8 clock gives a two-cycle PL pulse of about 83.3 ns
and a five-cycle nominal capture-close bound of about 208.3 ns from CEO,
excluding propagation and metastability. The **entire** CO/SO/DO word must
remain stable until PL closes. Require at least one further MCK period after
REQUEST_n before starting SPI. The bench checks the pulse length and that the
request follows PL closing; HP data hold and HCT165 recovery need measurement.

The capture pulse controller has four states: idle, first load cycle, second load
cycle and captured. The data word lives in the external HCT165s; there is no
additional input buffer. PL stays low for two MCK cycles, then goes high and
REQUEST_n goes low. PL comes directly from one stored state bit.
`capture_reset_n` clears this controller on reset, HP_RUN low, CFG_FRAME high or
CS low. A subsequent selected CEO edge reloads the HCT165s even if the previous
request has not been read; there is no unread-word lock or overrun flag. CS low
inhibits all loads during shifting, as Hilpert's CTL does. Firmware services the
current request promptly using the HP handshake. Physical capture/reset-release
timing remains part of issue 005.

The last MCK process uses two registers (`enable_meta`, `enable_sync`) to
synchronize the asynchronous selected CEO request and a third (`enable_prev`)
to detect one assertion. Fast Mode can hold CEO active until CFI; it must not
cause repeated captures. The process only synchronizes/detects the request; it
does not track overruns. This replaces Hilpert's analog monostable with a finite
clocked load pulse while keeping the same input-load inhibit during transfers.

The user's speed proposal is 24 MHz SPI (0.667 us per word). ST DS5319 Table 43
specifies a maximum of 18 MHz, which SPI2 can derive from 36 MHz APB1 with /2;
a /4 starting rate of 9 MHz takes 1.778 us per word and gives more HCT165 timing
margin. Use 9 MHz provisionally, then validate any increase in issue 028.
The bench exercises 9, 18 and 24 MHz with an ideal HCT165 model; passing 24 MHz
simulation does not establish that the MCU or shift registers support that rate.
The independent PA8 capture clock remains 24 MHz.

Fast Mode service should
target a response in the tens of microseconds after the request unless a confirmed
tape command requires a deliberate delay. OLED refresh and SD transactions must
not block that path. This design follows the user's decision to rely on timely
STM32 service, with no hardware overrun detector or queue. Hilpert explicitly
notes that some unacknowledged Fast Mode operations require prompt host service;
their command-specific scheduling remains in issue 007. Normal acknowledged
transfers use busy/ready handshaking, rather than a comparison of clock rates alone.
