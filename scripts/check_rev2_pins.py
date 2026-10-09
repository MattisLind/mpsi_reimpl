#!/usr/bin/env python3
"""Cross-check package pins, inline fitter constraints and exported KiCad nets."""
import argparse
import json
from pathlib import Path
import re
from kicad_sexpr import read, child, children

ROOT = Path(__file__).resolve().parents[1]
MAP = json.loads((ROOT / 'hardware/rev2/pinmap.json').read_text())
SPECIAL = {'VCC', 'GND', 'TDI', 'TMS', 'TCK', 'TDO'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fit', type=Path)
    parser.add_argument('--netlist', type=Path)
    parser.add_argument('--board', type=Path, help='also compare PCB pad nets to the exported netlist')
    args = parser.parse_args()
    ports = {name:int(pin) for pin,(name,net) in MAP['cpld'].items()
             if name not in SPECIAL and not name.startswith('SPARE')}
    source = (ROOT / 'hardware/rev2/rtl/mpsi_cpld.vhdl').read_text()
    inline = {name:int(pin) for name,pin in re.findall(r'^--PIN: (\w+) : (\d+)$', source, re.M)}
    jtag = {name:int(pin) for pin,(name,net) in MAP['cpld'].items() if name in ('TDI','TMS','TCK','TDO')}
    assert inline == ports | jtag, 'VHDL --PIN constraints disagree with pinmap.json'
    assert len(ports) == len(set(ports.values())) == 29, 'duplicate/missing CPLD pins'
    assert ports['reset_n'] == 1 and ports['hp_run'] == 44
    assert {ports['mck'],ports['spi_sck'],ports['spi_cs_n']} == {43,2,41}, 'global clock pins'
    assert {p for p,v in MAP['cpld'].items() if v[0]=='VCC'} == {'3','15','23','35'}
    assert {p for p,v in MAP['cpld'].items() if v[0]=='GND'} == {'10','22','30','42'}
    assert {p for p,v in MAP['cpld'].items() if v[0] in ('TDI','TMS','TCK','TDO')} == {'7','13','32','38'}
    assert {v[0] for v in MAP['mcu'].values() if v[2]=='in_5v_ft'} == {'PB14','PB10'}
    assert MAP['mcu']['30'][0:2] == ['PA9','DEBUG_TX']
    assert MAP['mcu']['31'][0:2] == ['PA10','DEBUG_RX']
    print('PASS: 29 CPLD constraints, global clocks/JTAG/supplies, FT inputs and USART1 reservation')
    if args.fit:
        fitted = {name:int(pin) for name,pin in re.findall(r'^\s*(\w+)\s*:\s*(\d+)', args.fit.read_text(), re.M) if name in ports}
        assert fitted == ports, 'fitter changed or omitted requested package pins'
        jed = args.fit.with_suffix('.jed')
        assert jed.is_file() and jed.stat().st_size > 0, 'no JEDEC generated'
        report = args.fit.with_suffix('.fit').read_text()
        assert '$Device PLCC44 fits; JTAG ON;' in report, 'fitter did not report a fit with JTAG'
        assert re.search(r'Pin-Keeper\s*=\s*OFF', report), 'pin keepers must be disabled'
        hp = {name:pin for name,pin in ports.items() if name.startswith('hp_n_')}
        open_collector = {name:int(pin) for pin,name in re.findall(
            r'^MC\d+\s+(\d+)\s+.*?\b(hp_n_\w+)\b.*\sfo\s*$', report, re.M)}
        assert open_collector == hp, 'HP outputs were not all fitted open collector'
        mapped = len(re.findall(r'\(cellRef DFF\w*', args.fit.with_suffix('.edif').read_text()))
        fitted_registers = re.search(r'Total Flip-Flop used\s+(\d+)/64', report)
        assert fitted_registers and int(fitted_registers[1]) == mapped, 'fitter lost registers'
        print(f'PASS: retained pins, JEDEC, JTAG, 14 open-collector HP outputs, no keepers, {mapped} registers')
    if args.netlist:
        tree = read(args.netlist)
        nets, pins = {}, {}
        for net in children(child(tree,'nets'),'net'):
            name = child(net,'name')[1]
            nodes = {(child(n,'ref')[1],child(n,'pin')[1]) for n in children(net,'node')}
            nets[name] = nodes
            pins.update({node:name for node in nodes})
        for ref, mapping in [('U1',{p:v[1] for p,v in MAP['mcu'].items()}),
                             ('U2',{p:v[1] for p,v in MAP['cpld'].items()}),
                             ('J1',MAP['connector']),
                             ('J5',{str(i):v for i,v in enumerate(MAP['debug_header'],1)})]:
            for pin,net in mapping.items():
                actual = pins.get((ref,pin))
                if net is None:
                    assert actual is None or actual.startswith('unconnected-'), f'{ref}.{pin} should be NC'
                else:
                    assert actual == net, f'{ref}.{pin}: expected {net}, got {actual}'
        expected = {
          'HP_SPI_SCK': {('U1','26'),('U2','2'),('U3','2'),('U4','2')},
          'HP_SPI_MISO': {('U1','27'),('U4','9')},
          'SHIFT_CHAIN': {('U3','9'),('U4','10')},
          'HP_INPUT_PL_n': {('U2','31'),('U3','1'),('U4','1')},
          'CPLD_MCK': {('U1','29'),('U2','43')},
          'USB_CONN_DM': {('J2','5'),('J2','7'),('U5','6')},
          'USB_CONN_DP': {('J2','6'),('J2','8'),('U5','4')},
          'USB_ESD_DM': {('U5','1'),('R21','2')},
          'USB_ESD_DP': {('U5','3'),('R22','2')},
          'USB_DM': {('U1','32'),('R21','1')},
          'USB_DP': {('U1','33'),('R22','1'),('R23','2')},
          'USB_PULLUP_EN': {('U1','41'),('R23','1'),('R24','1')},
          'USB_CC1': {('J2','4'),('R19','1')},
          'USB_CC2': {('J2','10'),('R20','1')},
          'SD_CD_n': {('U1','40'),('J3','10'),('R8','1')},
          'GND': {('J3','SH'),('U6','3'),('U6','7')},
          '+3V3': {('U6','1')}, '+5V_LOGIC': {('U6','6'),('U6','4')}
        }
        for name,nodes in expected.items():
            assert nodes <= nets[name], f'missing/miswired connection on {name}'
        for bit,pin in enumerate(['11','12','13','14','3','4','5','6']):
            assert pins[('U3',pin)] == 'DO'+str(bit), 'lower HCT165 byte order'
            assert pins[('U4',pin)] == ('SO'+str(bit) if bit < 4 else 'CO'+str(bit-4)), 'upper HCT165 word order'
        print('PASS: KiCad MCU/CPLD/connector/debug pins, HCT165 word order, USB protection and SD detect')
        if args.board:
            board = read(args.board)
            count = 0
            for footprint in children(board, 'footprint'):
                ref = next(p[2] for p in children(footprint,'property') if p[1]=='Reference')
                for pad in children(footprint,'pad'):
                    if not pad[1]: continue # mounting hole
                    actual = child(pad,'net')
                    actual = actual[-1] if actual else None
                    assert actual == pins.get((ref,pad[1])), f'PCB {ref}.{pad[1]}: {actual} != schematic'
                    count += 1
            print(f'PASS: {count} PCB pads match schematic nets, including NC and shield pads')
    elif args.board:
        parser.error('--board requires --netlist')


if __name__ == '__main__':
    main()
