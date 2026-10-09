# Existing connector reference

Extracted from the current [KiCad PCB](../MPSIBOARD/MPSIBOARD.kicad_pcb) and
cross-checked against J1's symbol/footprint in the
[current schematic](../MPSIBOARD/MPSIBOARD.kicad_sch), 2026-10-08.
The PCB contains embedded pad/net assignments; exporting a separate netlist is
not necessary to read these connections. Legacy hardware files are unchanged.

J1 footprint: `Connector:EDAC 2x25 3.96mm`, at (41.327, 37.923) mm,
rotated -90 degrees. Pad row **a is B.Cu**, row **b is F.Cu**. These are KiCad
pad labels and board sides, not Hilpert's letter/number connector convention.
Retain the footprint's orientation and full connector outline when reusing it.

| Position | a: B.Cu | b: F.Cu |
| --- | --- | --- |
| 1 | NC | GND |
| 2 | nCFI | nCEO |
| 3 | nSIH | nSSI |
| 4 | CO3 | CO2 |
| 5 | CO1 | CO0 |
| 6 | nSI3 | SO3 |
| 7 | nSI2 | SO2 |
| 8 | nSI1 | SO1 |
| 9 | nSI0 | SO0 |
| 10 | nDI7 | DO7 |
| 11 | nDI6 | DO6 |
| 12 | nDI5 | DO5 |
| 13 | nDI4 | DO4 |
| 14 | nDI1 | nDI2 |
| 15 | DO3 | nDI3 |
| 16 | DO2 | DO1 |
| 17 | DO0 | nDI0 |
| 18 | NC | NC |
| 19 | NC | NC |
| 20 | NC | NC |
| 21 | +5V | +5V |
| 22 | GND | GND |
| 23 | NC | NC |
| 24 | NC | NC |
| 25 | NC | NC |

NC means unconnected in the existing design, not permission to drive a reserved
HP contact. The existing contact assignment includes all CO/SO/DO inputs and
DI/SI/CFI/SSI outputs required by Rev2. It does not route HP MCK; Rev2 uses PA8's
TIM1 clock instead.

The footprint has 25 contacts on each side, 3.96 mm pitch, and pad size
9.00 x 2.65 mm in local footprint coordinates. Pad centres span 95.04 mm;
positions 23..25 are unconnected. The PCB's main rectangle runs from x=41.380
to 179.737 mm and y=36.843 to 134.043 mm, or 138.357 x 97.200 mm. Connector
extensions and outline segments also exist in the footprint and adjacent
Edge.Cuts; that rectangle alone is not the complete board outline.

The older [legacy schematic](../MPSIBOARD/MPSIBOARD.sch) uses a 2x22 symbol,
whereas the current schematic and PCB use 2x25. Use the current files as the
geometry/assignment baseline requested by the user. Hilpert's PDF labels the
connector from the opposite numbering direction; compare signals, not identical
position numbers, when consulting it. These extracted dimensions establish the
available reference, not a new physical-fit measurement.

