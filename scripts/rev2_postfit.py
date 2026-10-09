#!/usr/bin/env python3
"""Translate the fitter's VHO primitives for a zero-delay functional GHDL check.

This checks the fitter-emitted equations, not the JEDEC fuse encoding or SDF
timing. Only explicitly supported primitives are accepted; no RTL is substituted.
"""
from pathlib import Path
import re
import sys

root, fit, output = map(Path, sys.argv[1:])
output.mkdir(parents=True, exist_ok=True)
model = (fit / 'mpsi_cpld.vho').read_text()
model = re.sub(r'LIBRARY atml_vtl;\s*USE atml_vtl.VCOMPONENTS.all;', '', model, flags=re.I)
model = re.sub(r'\bmpsi_cpld\b', 'mpsi_fitted', model, flags=re.I)
counts = {}


def primitive(match):
    label, kind, mapping = match.groups()
    ports = dict(re.findall(r'(\w+)\s*=>\s*(\w+)', mapping))
    kind = kind.upper()
    counts[kind] = counts.get(kind, 0) + 1
    if kind == 'BUF':
        expected, equation = {'Q', 'A'}, f"{ports['Q']} <= {ports['A']};"
    elif kind == 'INV':
        expected, equation = {'QN', 'A'}, f"{ports['QN']} <= not {ports['A']};"
    elif kind == 'TRI':
        expected = {'Q', 'A', 'EN'}
        equation = f"{ports['Q']} <= {ports['A']} when {ports['EN']} = '1' else 'Z';"
    elif kind == 'DFFEARS':
        expected = {'Q', 'D', 'CLK', 'AR', 'AS', 'CE'}
        assert ports['AS'].lower() == 'gnd', 'inspect nonzero asynchronous preset'
        equation = f"""{label}: process({ports['CLK']}, {ports['AR']}, {ports['AS']})
begin
    if {ports['AR']} = '1' then {ports['Q']} <= '0';
    elsif {ports['AS']} = '1' then {ports['Q']} <= '1';
    elsif rising_edge({ports['CLK']}) then
        if {ports['CE']} = '1' then {ports['Q']} <= {ports['D']}; end if;
    end if;
end process;"""
    else:
        gate = re.fullmatch(r'(AND|OR|XOR)(\d+)', kind)
        assert gate, f'unsupported fitter primitive: {kind}'
        operator, size = gate[1].lower(), int(gate[2])
        expected = {'Q'} | {f'A{i}' for i in range(1, size + 1)}
        equation = f"{ports['Q']} <= " + f' {operator} '.join(ports[f'A{i}'] for i in range(1, size + 1)) + ';'
    assert set(ports) == expected, f'unexpected ports on {label}: {ports}'
    return equation


model = re.sub(r'^(\w+):\s+(\w+)\s+PORT MAP\s*\(([^;]+)\);', primitive, model, flags=re.M)
assert 'PORT MAP' not in model.upper(), 'untranslated fitter instance'
mapped = len(re.findall(r'\(cellRef DFF\w*', (fit / 'mpsi_cpld.edif').read_text()))
assert counts.get('DFFEARS') == mapped, 'fitter changed the register count'
(output / 'mpsi_fitted.vhdl').write_text('-- Zero-delay fitter equations; SDF is not applied.\n' + model)
rtl = (root / 'hardware/rev2/rtl/mpsi_cpld.vhdl').read_text()
interface = rtl[rtl.index('library ieee;'):rtl.index('architecture rtl')]
names = re.findall(r'^\s+(\w+)\s*:\s*(?:in|out) std_logic(?:;|\))', model, flags=re.M | re.I)
assert len(names) == 29, 'unexpected fitter interface'
connections = []
for name in names:
    vector = re.fullmatch(r'(co|hp_n_di)_(\d+)', name)
    actual = f'{vector[1]}({vector[2]})' if vector else name
    connections.append(f'{name} => {actual}')
adapter = interface + 'architecture fitted of mpsi_cpld is\nbegin\n'
adapter += '    dut: entity work.mpsi_fitted port map(\n        ' + ',\n        '.join(connections) + ');\nend architecture;\n'
(output / 'mpsi_adapter.vhdl').write_text(adapter)
print(f'Translated fitter equations: {mapped} registers, {len(names)} pins; no SDF timing')
