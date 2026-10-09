# Package pins and tentative placement

2026-10-09, revision tentative-3. The simplified implementation fits with all
29 requested logical pins retained. Electrical, timing and routing validation
remain open; this is a board draft, not manufacturing sign-off.
Keep schematic/board and inline VHDL constraints synchronized through
[`pinmap.json`](../hardware/rev2/pinmap.json) and the pin/net checker.

Use STM32F103CBT6 in LQFP48 and industrial ATF1504AS-10JU44 in PLCC44.
The MCU package tables use [ST DS5319, Rev 20](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf);
CPLD supplies, JTAG and dedicated inputs use [the ATF1504 datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-0950-CPLD-ATF1504AS(L)-Datasheet.pdf).

## STM32 assignment

| Function | GPIO (package pin) | Routing / initialization |
| --- | --- | --- |
| HP SPI2 | PB13 SCK (26), PB14 MISO (27), PB15 MOSI (28), PB12 CS (25) | Mode 2; no remapping; PB14 is a direct FT input. |
| CPLD controls | PB1 CFG_FRAME (19), PB8 HP_RUN (45), PB9 RESET_n (46) | 3.3 V outputs; external default-low pulls. PB0 is spare after removing COMMIT. |
| CPLD request | PB10 REQUEST_n (21) | Direct FT input; EXTI10 request; internal pulls disabled. PB11 is spare after removing overrun. |
| Capture clock | PA8 TIM1_CH1 (29) | 24 MHz from 72 MHz timer, PSC=0 ARR=2 CCR1=1. |
| SD SPI1 | PA4 CS (14), PA5 SCK (15), PA6 MISO (16), PA7 MOSI (17) | Dedicated SPI1; all signals at 3.3 V. |
| SD detection | PB4 (40) | Active low to ground, 10 kOhm pull-up to 3.3 V; debounce/rescan. |
| OLED I2C1 | PB6 SCL (42), PB7 SDA (43) | 4.7 kOhm to 3.3 V; check module pull-ups and address. |
| Buttons | PA0 up (10), PA1 down (11), PA2 select (12) | Active low, 10 kOhm pull-ups; software debounce. |
| USB | PA11 DM (32), PA12 DP (33), PB5 pull-up enable (41), PA3 VBUS sense (13) | PA3 receives the divided voltage, not 5 V. PB5 drives 1.5 kOhm D+ pull-up. |
| Serial debug | PA9 TX (30), PA10 RX (31) | USART1; J5 order: 1=+3.3 V, 2=TX, 3=RX, 4=GND. 3.3 V UART levels. |
| Debug / oscillator | PA13 SWDIO (34), PA14 SWCLK (37), PD0/PD1 HSE (5/6) | Keep SWD; 8 MHz crystal proposal. |

PA6 is **not** FT and therefore serves the 3.3 V SD MISO. PB14 is FT and
resets as an ordinary GPIO, so SPI2 is used for the 5 V HP input chain.
The earlier remapped-SPI1 proposal used PB4 MISO, but PB4's reset-state JNTRST
pull-up complicates direct 5 V operation. This map keeps PB4 at 3.3 V for card
detect, and PB5 (non-FT) drives the USB pull-up. PA9 remains reserved for TX;
VBUS sense uses PA3. MCU outputs must satisfy the CPLD/HCT TTL input thresholds
at worst-case voltage and loading.

Firmware uses SPI2 for the HP link and SPI1 for SD, without SPI remapping.
Set `AFIO_MAPR.SWJ_CFG=010` (JTAG off, SWD on) to release PB4 for card detect.
PB0/PB3/PB11 are spare; SWO can be considered later. Configure PB14/PB10 as floating
digital inputs with no internal pull-up/down before applying 5 V signals.

FT operation is specified with the MCU powered in its stated supply range.
The absolute FT input bound depends on VDD (`VDD + 4 V`); this does not make
a 5 V source safe when VDD is zero. The direct 5 V pins reset without the JNTRST pull-up complication. Check the supply ramp, reset and fault cases for
the CPLD request, HCT165 MISO and raw-VBUS divider. Direct connections
are tentative until issue 026 is resolved; isolation may still be needed.
Do not assume the Schottky supply topology alone proves safe sequencing.

All MCU package pins:

| Pin | STM32 signal | Board net | Use |
| --- | --- | --- | --- |
| 1 | VBAT | +3V3 | power |
| 2 | PC13 | NC | spare |
| 3 | PC14 | NC | spare |
| 4 | PC15 | NC | spare |
| 5 | PD0 | HSE_IN | clock |
| 6 | PD1 | HSE_OUT | clock |
| 7 | NRST | MCU_RESET_n | in |
| 8 | VSSA | GND | power |
| 9 | VDDA | +3V3 | power |
| 10 | PA0 | BUTTON_UP_n | in |
| 11 | PA1 | BUTTON_DOWN_n | in |
| 12 | PA2 | BUTTON_SELECT_n | in |
| 13 | PA3 | USB_VBUS_SENSE | in |
| 14 | PA4 | SD_CS_n | out |
| 15 | PA5 | SD_SCK | out |
| 16 | PA6 | SD_MISO | in |
| 17 | PA7 | SD_MOSI | out |
| 18 | PB0 | NC | spare |
| 19 | PB1 | CPLD_CFG_FRAME | out |
| 20 | PB2 | BOOT1 | in |
| 21 | PB10 | CPLD_REQUEST_n | in_5v_ft |
| 22 | PB11 | NC | spare |
| 23 | VSS1 | GND | power |
| 24 | VDD1 | +3V3 | power |
| 25 | PB12 | CPLD_CS_n | out |
| 26 | PB13 | HP_SPI_SCK | out |
| 27 | PB14 | HP_SPI_MISO | in_5v_ft |
| 28 | PB15 | HP_SPI_MOSI | out |
| 29 | PA8 | CPLD_MCK | out |
| 30 | PA9 | DEBUG_TX | out |
| 31 | PA10 | DEBUG_RX | in |
| 32 | PA11 | USB_DM | bidirectional |
| 33 | PA12 | USB_DP | bidirectional |
| 34 | PA13 | SWDIO | bidirectional |
| 35 | VSS2 | GND | power |
| 36 | VDD2 | +3V3 | power |
| 37 | PA14 | SWCLK | in |
| 38 | PA15 | NC | spare |
| 39 | PB3 | NC | spare |
| 40 | PB4 | SD_CD_n | in |
| 41 | PB5 | USB_PULLUP_EN | out |
| 42 | PB6 | OLED_SCL | bidirectional |
| 43 | PB7 | OLED_SDA | bidirectional |
| 44 | BOOT0 | BOOT0 | in |
| 45 | PB8 | CPLD_HP_RUN | out |
| 46 | PB9 | CPLD_RESET_n | out |
| 47 | VSS3 | GND | power |
| 48 | VDD3 | +3V3 | power |

## CPLD assignment

The three clock domains use GCLK2 pin 2 (SPI SCK), GCLK3 pin 41 (CS rising for flags),
and GCLK1 pin 43 (MCK). Reset uses GCLR pin 1, HP_RUN uses dedicated input/OE1
pin 44, and JTAG retains pins 7/13/32/38. Pins 34/39/40 are spare. CS moved from
pin 40 to the global clock pin freed by removing COMMIT.
HP DI0..7 and SI0..3 correspond to `hp_n_di_0` through `hp_n_di_11`.
All fourteen HP outputs are 0/Z in VHDL and must be fitted open collector with
pin keepers off. MCU-facing REQUEST_n and HCT PL are push-pull at 5 V.

| PLCC pin | RTL / package function | Board net |
| --- | --- | --- |
| 1 | reset_n | CPLD_RESET_n |
| 2 | spi_sck | HP_SPI_SCK |
| 3 | VCC | +5V_LOGIC |
| 4 | hp_n_cfi | nCFI |
| 5 | co_0 | CO0 |
| 6 | co_1 | CO1 |
| 7 | TDI | CPLD_TDI |
| 8 | co_2 | CO2 |
| 9 | co_3 | CO3 |
| 10 | GND | GND |
| 11 | nceo | nCEO |
| 12 | nsih | nSIH |
| 13 | TMS | CPLD_TMS |
| 14 | hp_n_ssi | nSSI |
| 15 | VCC | +5V_LOGIC |
| 16 | hp_n_di_11 | nSI3 |
| 17 | hp_n_di_10 | nSI2 |
| 18 | hp_n_di_9 | nSI1 |
| 19 | hp_n_di_8 | nSI0 |
| 20 | hp_n_di_7 | nDI7 |
| 21 | hp_n_di_6 | nDI6 |
| 22 | GND | GND |
| 23 | VCC | +5V_LOGIC |
| 24 | hp_n_di_5 | nDI5 |
| 25 | hp_n_di_4 | nDI4 |
| 26 | hp_n_di_3 | nDI3 |
| 27 | hp_n_di_2 | nDI2 |
| 28 | hp_n_di_1 | nDI1 |
| 29 | hp_n_di_0 | nDI0 |
| 30 | GND | GND |
| 31 | input_pl_n | HP_INPUT_PL_n |
| 32 | TCK | CPLD_TCK |
| 33 | request_n | CPLD_REQUEST_n |
| 34 | SPARE34 | NC |
| 35 | VCC | +5V_LOGIC |
| 36 | spi_mosi | HP_SPI_MOSI |
| 37 | cfg_frame | CPLD_CFG_FRAME |
| 38 | TDO | CPLD_TDO |
| 39 | SPARE39 | NC |
| 40 | SPARE40 | NC |
| 41 | spi_cs_n | CPLD_CS_n |
| 42 | GND | GND |
| 43 | mck | CPLD_MCK |
| 44 | hp_run | CPLD_HP_RUN |

VHDL carries DECPROMEM-style constraints, for example:

```vhdl
--PIN: CHIP "mpsi_cpld" ASSIGNED TO AN PLCC44
--PIN: spi_sck : 2
--PIN: spi_cs_n : 41
--PIN: mck : 43
```

The build uses `-preassign keep`. A successful fitter result is accepted only
when all 29 logical pins remain assigned as requested and JEDEC exists.
The simplified implementation passes this check and retains JTAG, fourteen
open-collector HP outputs and disabled pin keepers. The report has inconsistent
resource totals despite declaring a fit; issue 029 records the accounting review.
See [build notes](../hardware/rev2/README.md) and issue 004.

## Placement and external connections

The draft copies the legacy connector position and complete board outline.
The coordinate origin is the KiCad board origin; dimensions are millimetres.
These component centres/orientations are tentative:

| Component | X | Y | Rotation |
| --- | --- | --- | --- |
| U1 | 118 | 80 | 90 degrees |
| U2 | 82 | 78 | 90 degrees |
| U3 | 63 | 94 | 0 degrees |
| U4 | 63 | 59 | 0 degrees |
| J2 | 171 | 113 | 90 degrees |
| J3 | 160 | 80 | 90 degrees |
| J4 | 143 | 43 | 0 degrees |
| J5 | 151 | 96 | 0 degrees |

U3 captures DO0..7; U4 captures SO0..3 and CO0..3. U3 Q7 feeds U4 DS;
U4 non-inverting Q7 feeds PB14 directly. Both CE pins are low, CP uses PB13
and PL uses CPLD pin 31. See the exact word/pad order in
[02_hardware.md](02_hardware.md) and [06_existing_connector.md](06_existing_connector.md).

Keep CPLD/HCT165 traces to the HP connector short. Route controls/clock/SPI
across the MCU-facing CPLD side, place decouplers beside the actual supply
pins, and keep the USBLC6 beside USB-C with short connector-to-protection paths.
Keep USB D+/D- together through protection/22 Ohm series resistors to PA11/PA12.
The board is unrouted; connector orientation, enclosure access, crystal loading
and production clearances remain open in issue 027.

SPI2 provisionally uses 9 MHz from APB1=36 MHz /4. The proposed 24 MHz SPI is
above the STM32F103's stated 18 MHz ceiling. Validate HCT165 cascade and MISO
timing before raising the rate; see issue 028. The PA8 capture clock stays 24 MHz.

USB-C uses the same HRO-TYPE-C-31-M-12 symbol/footprint and USBLC6-2SC6 circuit
as the pinned paper-tape project (source S15). Each CC pin has a 5.1 kOhm sink
resistor; each data line has 22 Ohm series resistance. USBLC6 power/clamp uses
raw USB VBUS. The 1.5 kOhm D+ pull-up is controlled by PB5, with an added
10 kOhm default-low pull. The 1.8 kOhm / 3.3 kOhm VBUS divider feeds PA3.
Keep the reference power-source diodes and TLV75533 footprint, subject to the
CPLD industrial 4.50 V rail budget in issue 022.
