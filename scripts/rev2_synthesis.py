#!/usr/bin/env python3
"""Use the same synthesis sequence as the selected VHDL container wrapper."""
from pathlib import Path
import re
import sys

root, output = map(Path, sys.argv[1:])
# aprim.lib calls TRI's active-high enable ENA, while the Yosys library calls
# it EN. Align a build-local copy; otherwise Yosys treats ENA as an extra port
# and removes its driver, or the Atmel fitter rejects EN. Leave tools untouched.
library = (root / 'cells.lib').read_text()
start = library.index('cell(TRI)')
opening = library.index('{', start)
depth, closing = 1, opening + 1
while depth:
    depth += (library[closing] == '{') - (library[closing] == '}')
    closing += 1
block = library[start:closing]
block, changes = re.subn(r'pin\(EN\)', 'pin(ENA)', block)
assert changes == 1, 'unexpected TRI definition; inspect tool library before synthesis'
(output / 'atf_cells.lib').write_text(library[:start] + block + library[closing:])
for suffix in ('jed', 'fit', 'pin', 'tt3', 'io', 'vho', 'sdo'):
    (output / ('mpsi_cpld.' + suffix)).unlink(missing_ok=True)
(output / 'mpsi.ys').write_text(f'''read_liberty -lib atf_cells.lib
ghdl --std=08 mpsi_cpld.vhdl -e mpsi_cpld
stat
tribuf
synth -flatten -noabc -top mpsi_cpld
techmap -map {root}/techmap.v -D skip_DFFE_XX_
simplemap
dfflibmap -liberty atf_cells.lib
abc -liberty atf_cells.lib
iopadmap -bits -inpad INBUF Q:A -outpad BUF A:Q -toutpad TRI ENA:A:Q -tinoutpad bibuf EN:Q:A:PAD
clean
hierarchy
splitnets -format _
rename -wire -suffix _reg t:*DFF*
rename -wire -suffix _comb
check -assert
write_edif -lsbidx mpsi_cpld.edif
''')
