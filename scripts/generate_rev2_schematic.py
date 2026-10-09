#!/usr/bin/env python3
"""Generate the tentative Rev2 schematic from its reviewed package-pin map.

Embedded symbols and local connector/USB footprints make the saved project
self-contained. Regeneration requires KiCad libraries; editing the schematic
manually is also supported. Netlist verification uses check_rev2_pins.py.
"""
import argparse
import copy
import json
from pathlib import Path
import uuid

from kicad_sexpr import Atom as A, child, children, read, write

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'hardware/rev2/kicad'
MAP = json.loads((ROOT / 'hardware/rev2/pinmap.json').read_text())
LIBRARY = Path('/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols')
REFERENCE = Path('/tmp/mpsi-hp21xx.kicad_sch')
REFERENCE_PCB = Path('/tmp/mpsi-hp21xx.kicad_pcb')
NAMESPACE = uuid.UUID('13575163-3b34-4a3d-9b2a-983015040103')


def uid(name):
    return str(uuid.uuid5(NAMESPACE, name))


def e(tag, *args):
    return [A(tag), *args]


def num(value):
    return A(f'{value:g}')


def point(tag, x, y, angle=None):
    return e(tag, num(x), num(y), *([] if angle is None else [num(angle)]))


def effects(size=1.0, hide=False):
    return e('effects', e('font', e('size', num(size), num(size))), *([e('hide', A('yes'))] if hide else []))


def rename_units(symbol, old, new):
    for unit in children(symbol, 'symbol'):
        unit[1] = unit[1].replace(old + '_', new + '_', 1)


def library_symbol(lib_id):
    library, name = lib_id.split(':', 1)
    tree = read(LIBRARY / (library + '.kicad_sym'))
    symbols = {s[1]: s for s in children(tree, 'symbol')}

    def resolve(name):
        symbol = copy.deepcopy(symbols[name])
        parent = child(symbol, 'extends')
        if parent:
            base = resolve(parent[1])
            rename_units(base, parent[1], name)
            for tag in ('property', 'symbol'):
                replacements = {x[1] for x in children(symbol, tag)}
                base[:] = [x for x in base if not (isinstance(x, list)
                         and x[0] == tag and x[1] in replacements)]
            base[1] = name
            base.extend(x for x in symbol[2:] if x[0] != 'extends')
            symbol = base
        return symbol

    result = resolve(name)
    result[1] = lib_id
    return result


def embedded(tree, lib_id):
    return copy.deepcopy(next(s for s in children(child(tree, 'lib_symbols'), 'symbol') if s[1] == lib_id))


def cpld_symbol():
    name = 'ATF1504AS_PLCC44'
    result = e('symbol', 'Rev2:' + name, e('pin_names', e('offset', num(1.016))),
               e('in_bom', A('yes')), e('on_board', A('yes')),
               e('property', 'Reference', 'U', point('at', 0, 38, 0), effects()),
               e('property', 'Value', MAP['cpld_part'], point('at', 0, -38, 0), effects()),
               e('property', 'Footprint', 'Package_LCC:PLCC-44_16.6x16.6mm_P1.27mm',
                 point('at', 0, 0, 0), effects(hide=True)))
    body = e('symbol', name + '_0_1', e('rectangle', point('start', -20.32, 35.56),
             point('end', 20.32, -35.56), e('stroke', e('width', num(0.254)), e('type', A('default'))),
             e('fill', e('type', A('background')))))
    pins = e('symbol', name + '_1_1')
    bus = ['co_3', 'co_2', 'co_1', 'co_0', 'nceo', 'nsih', 'hp_n_cfi', 'hp_n_ssi']
    bus += ['hp_n_di_' + str(i) for i in range(11, -1, -1)]
    control = ['mck', 'spi_sck', 'spi_mosi', 'spi_cs_n', 'cfg_frame',
               'reset_n', 'hp_run', 'input_pl_n', 'request_n', 'TCK', 'TMS', 'TDI', 'TDO']
    control += [p for p,n in MAP['cpld'].values() if p.startswith('SPARE')]
    for pin, (port, net) in MAP['cpld'].items():
        if port in bus:
            x, y, angle = -25.4, 27.94 - bus.index(port) * 2.54, 0
        elif port in control:
            x, y, angle = 25.4, 27.94 - control.index(port) * 2.54, 180
        else:
            supply = [p for p, v in MAP['cpld'].items() if v[0] == port]
            x = -7.62 + supply.index(pin) * 5.08
            y, angle = (40.64, 270) if port == 'VCC' else (-40.64, 90)
        typ = ('power_in' if port in ('VCC', 'GND') else 'open_collector' if port.startswith('hp_n_')
               else 'output' if port in ('request_n', 'input_pl_n', 'TDO')
               else 'passive' if port.startswith('SPARE') else 'input')
        label = port if port in ('VCC', 'GND', 'TDI', 'TMS', 'TCK', 'TDO') or port.startswith('SPARE') else net
        pins.append(e('pin', A(typ), A('line'), point('at', x, y, angle), e('length', num(5.08)),
                    e('name', label, effects()), e('number', pin, effects())))
    result.extend([body, pins])
    return result


class Sheet:
    def __init__(self, name, title, page, path):
        self.name, self.page, self.path = name, page, path
        self.uuid = uid('sheet/' + name)
        self.root = e('kicad_sch', e('version', A('20250114')), e('generator', A('eeschema')),
                      e('generator_version', '10.0'), e('uuid', self.uuid), e('paper', 'A3'),
                      e('title_block', e('title', title), e('date', '2026-10-09'),
                        e('rev', MAP['revision']), e('company', 'MPSI Rev2'),
                        e('comment', A('1'), 'Draft: SPI/capture timing, rail budget and validation pending')),
                      e('lib_symbols'))
        self.used = set()
        self.components = []

    def text(self, text, x, y, size=1.27):
        self.root.append(e('text', text, point('at', x, y, 0), effects(size), e('uuid', uid(self.name + text))))

    def global_net(self, name, x, y, angle):
        self.root.append(e('global_label', name, e('shape', A('input')), point('at', x, y, angle),
                           effects(0.9), e('uuid', uid(f'{self.name}/label/{name}/{x}/{y}'))))

    def add(self, ref, lib_id, value, nets, x, y, footprint=None, unit=1):
        x, y = round(x / 1.27) * 1.27, round(y / 1.27) * 1.27
        symbol = SYMBOLS[lib_id]
        if lib_id not in self.used:
            child(self.root, 'lib_symbols').append(copy.deepcopy(symbol))
            self.used.add(lib_id)
        if footprint is None:
            prop = next((p for p in children(symbol, 'property') if p[1] == 'Footprint'), None)
            footprint = prop[2] if prop else ''
        unique = uid('component/' + ref)
        instance = e('symbol', e('lib_id', lib_id), point('at', x, y, 0), e('unit', A(str(unit))),
                     e('in_bom', A('no' if ref.startswith('#') else 'yes')),
                     e('on_board', A('no' if ref.startswith('#') else 'yes')), e('dnp', A('no')), e('uuid', unique),
                     e('property', 'Reference', ref, point('at', x, y-3, 0), effects()),
                     e('property', 'Value', value, point('at', x, y+3, 0), effects()),
                     e('property', 'Footprint', footprint, point('at', x, y, 0), effects(hide=True)),
                     e('instances', e('project', 'MPSI_REV2', e('path', self.path,
                         e('reference', ref), e('unit', A(str(unit)))))))
        power_groups = {}
        pin_tops = []
        for sub in children(symbol, 'symbol'):
            # Shared graphics (_0_*) and the requested symbol unit (_1_*).
            if int(sub[1].rsplit('_', 2)[1]) not in (0, unit):
                continue
            for pin in children(sub, 'pin'):
                number = child(pin, 'number')[1]
                px, py, angle = map(float, child(pin, 'at')[1:])
                px, py = x + px, y - py
                pin_tops.append(py)
                instance.append(e('pin', number, e('uuid', uid(ref + '/' + number))))
                net = nets.get(number)
                if net is None:
                    self.root.append(e('no_connect', point('at', px, py), e('uuid', uid(ref + '/nc/' + number))))
                    continue
                dx, dy = {0: (-5.08, 0), 180: (5.08, 0), 90: (0, 5.08), 270: (0, -5.08)}[int(angle)]
                qx, qy = round(px+dx, 6), round(py+dy, 6)
                self.root.append(e('wire', e('pts', point('xy', px, py), point('xy', qx, qy)),
                                   e('stroke', e('width', num(0)), e('type', A('default'))),
                                   e('uuid', uid(ref + '/wire/' + number))))
                if pin[1] == 'power_in' and angle in (90, 270):
                    power_groups.setdefault((net, angle, qy), []).append(qx)
                else:
                    self.global_net(net, qx, qy, 0 if angle in (0, 90, 270) else 180)
        for (net, angle, qy), xs in power_groups.items():
            lo, hi = min(xs), max(xs)
            if lo != hi:
                self.root.append(e('wire', e('pts', point('xy', lo, qy), point('xy', hi, qy)),
                    e('stroke', e('width', num(0)), e('type', A('default'))),
                    e('uuid', uid(f'{ref}/power/{net}/{angle}'))))
                for qx in set(xs):
                    self.root.append(e('junction', point('at', qx, qy), e('diameter', num(0)),
                        e('color', A('0'), A('0'), A('0'), A('0')),
                        e('uuid', uid(f'{ref}/powerjunction/{net}/{qx}/{qy}'))))
            self.global_net(net, lo, qy, 0)
        if lib_id.startswith(('Connector', 'Power_Protection', 'Regulator_Linear')) or 'USB_C_HRO' in lib_id:
            for prop in children(instance, 'property'):
                if prop[1] in ('Reference', 'Value'):
                    at = child(prop, 'at')
                    at[2] = num(min(pin_tops) - (5.08 if prop[1] == 'Reference' else 2.54))
        self.root.append(instance)
        self.components.append({'reference': ref, 'value': value, 'footprint': footprint,
                                'nets': nets, 'uuid': unique, 'path': self.path})

    def save(self):
        write(DEST / (self.name + '.kicad_sch'), self.root)


def footprint(source, reference, name):
    tree = read(source)
    candidates = children(tree, 'footprint') + children(tree, 'module')
    result = copy.deepcopy(next(f for f in candidates if
                  any(p[1:3] == ['Reference', reference] for p in children(f, 'property')) or
                  any(p[1:3] == [A('reference'), reference] for p in children(f, 'fp_text'))))
    # Board files store pad/text angles including the footprint rotation;
    # library files require relative angles. Preserve the existing geometry.
    origin = child(result, 'at')
    rotation = float(origin[3]) if origin and len(origin) > 3 else 0
    for item in children(result, 'pad') + children(result, 'fp_text') + children(result, 'property'):
        at = child(item, 'at')
        if at:
            if len(at) == 3: at.append(num(-rotation))
            else: at[3] = num(float(at[3]) - rotation)
    result[0] = A('footprint')
    result[1] = name
    result[:] = [x for x in result if not (isinstance(x, list) and x[0] in
                    ('at', 'path', 'uuid', 'tstamp', 'sheetfile', 'sheetname'))]
    result[:] = [x for x in result if not (isinstance(x, list) and x[0] == 'property'
                                          and x[1] in ('Sheetfile','Sheetname'))]
    for pad in children(result, 'pad'):
        pad[:] = [x for x in pad if not (isinstance(x, list) and x[0] in ('net', 'uuid', 'tstamp'))]
    for prop in children(result, 'property'):
        if prop[1] == 'Reference': prop[2] = 'REF**'
        elif prop[1] == 'Value': prop[2] = name
    for text in children(result, 'fp_text'):
        if text[1] == 'reference': text[2] = 'REF**'
        elif text[1] == 'value': text[2] = name
    result.insert(2, e('version', A('20241229')))
    result.insert(3, e('generator', 'pcbnew'))
    write(DEST / 'Rev2.pretty' / (name + '.kicad_mod'), result)


def main():
    global SYMBOLS, LIBRARY, REFERENCE, REFERENCE_PCB
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--symbols', type=Path, default=LIBRARY)
    parser.add_argument('--reference', type=Path, default=REFERENCE)
    parser.add_argument('--reference-pcb', type=Path, default=REFERENCE_PCB)
    args = parser.parse_args()
    LIBRARY, REFERENCE, REFERENCE_PCB = args.symbols, args.reference, args.reference_pcb
    DEST.mkdir(parents=True, exist_ok=True)
    (DEST / 'Rev2.pretty').mkdir(exist_ok=True)
    reference, original = read(REFERENCE), read(ROOT / 'MPSIBOARD/MPSIBOARD.kicad_sch')
    SYMBOLS = {}
    std = ['Device:R', 'Device:C', 'Device:D_Schottky', 'Device:Crystal_GND24', 'Switch:SW_Push',
           'Connector_Generic:Conn_01x02', 'Connector_Generic:Conn_01x04',
           'Connector_Generic:Conn_01x05', 'Connector_Generic:Conn_01x06', 'power:PWR_FLAG',
           'MCU_ST_STM32F1:STM32F103CBTx', '74xx:74HC165']
    for lib_id in std:
        SYMBOLS[lib_id] = library_symbol(lib_id)
    for lib_id in ['Power_Protection:USBLC6-2SC6', 'Regulator_Linear:TLV75533PDRV',
                   'Westfly:HRO-TYPE-C-31-M-12']:
        SYMBOLS[lib_id] = embedded(reference, lib_id)
    SYMBOLS['Connector:Micro_SD_Card_Det_Hirose_DM3AT'] = library_symbol('Connector:Micro_SD_Card_Det_Hirose_DM3AT')
    usb_symbol = SYMBOLS.pop('Westfly:HRO-TYPE-C-31-M-12')
    rename_units(usb_symbol, 'HRO-TYPE-C-31-M-12', 'USB_C_HRO_12')
    usb_symbol[1] = 'Rev2:USB_C_HRO_12'
    SYMBOLS[usb_symbol[1]] = usb_symbol
    lib_id = 'Connector_Generic:Conn_02x25_Row_Letter_First'
    SYMBOLS[lib_id] = embedded(original, lib_id)
    SYMBOLS['Rev2:ATF1504AS_PLCC44'] = cpld_symbol()
    write(DEST / 'Rev2.kicad_sym', e('kicad_symbol_lib', e('version', A('20241209')),
          e('generator', A('kicad_symbol_editor')), copy.deepcopy(SYMBOLS['Rev2:ATF1504AS_PLCC44']),
          copy.deepcopy(usb_symbol)))
    # Library symbol names omit the nickname when saved outside a schematic.
    local = read(DEST / 'Rev2.kicad_sym')
    for s in children(local, 'symbol'): s[1] = s[1].split(':', 1)[1]
    write(DEST / 'Rev2.kicad_sym', local)
    footprint(ROOT / 'MPSIBOARD/MPSIBOARD.kicad_pcb', 'J1', 'HP9830_EDAC_2x25')
    footprint(REFERENCE_PCB, 'USB1', 'HRO-TYPE-C-31-M-12')
    write(DEST / 'fp-lib-table', e('fp_lib_table', e('version', A('7')),
          e('lib', e('name', 'Rev2'), e('type', 'KiCad'),
            e('uri', '${KIPRJMOD}/Rev2.pretty'), e('options', ''), e('descr', 'Copied HP connector and USB-C footprints'))))
    write(DEST / 'sym-lib-table', e('sym_lib_table', e('version', A('7')),
          e('lib', e('name', 'Rev2'), e('type', 'KiCad'),
            e('uri', '${KIPRJMOD}/Rev2.kicad_sym'), e('options', ''), e('descr', 'ATF1504 PLCC44 pin mapping'))))

    root_id = uid('sheet/MPSI_REV2')
    bus = Sheet('MPSI_REV2', 'HP9830 bus, CPLD and input capture', 1, '/' + root_id)
    mcu = Sheet('mcu', 'STM32F103, SD card, UI and debug', 2, '/' + root_id + '/' + uid('hierarchy/mcu'))
    usb = Sheet('usb_power', 'USB-C, ESD protection and power', 3, '/' + root_id + '/' + uid('hierarchy/usb_power'))
    for sheet in (mcu, usb):
        x, y = (300, 75) if sheet is mcu else (300, 115)
        bus.root.append(e('sheet', point('at', x, y), e('size', num(75), num(22)),
            e('stroke', e('width', num(0.254)), e('type', A('default'))),
            e('fill', e('color', A('0'), A('0'), A('0'), A('0'))),
            e('uuid', uid('hierarchy/' + sheet.name)),
            e('property', 'Sheetname', sheet.name, point('at', x, y-2, 0), effects()),
            e('property', 'Sheetfile', sheet.name + '.kicad_sch', point('at', x, y+24, 0), effects()),
            e('instances', e('project', 'MPSI_REV2', e('path', '/' + root_id, e('page', str(sheet.page)))))))
    bus.root.append(e('sheet_instances', e('path', '/', e('page', '1'))))
    def add(sheet, ref, lib, val, nets, x, y, fp=None):
        sheet.add(ref, lib, val, {str(k): v for k, v in nets.items()}, x, y, fp)
    RFP = 'Resistor_SMD:R_0603_1608Metric'
    CFP = 'Capacitor_SMD:C_0603_1608Metric'
    def resistor(sheet, ref, val, first, second, x, y):
        add(sheet, ref, 'Device:R', val, {1:first, 2:second}, x, y, RFP)
    def capacitor(sheet, ref, val, rail, x, y):
        add(sheet, ref, 'Device:C', val, {1:rail, 2:'GND'}, x, y, CFP)
    def header(sheet, ref, val, nets, x, y):
        count = len(nets)
        add(sheet, ref, f'Connector_Generic:Conn_01x0{count}', val,
            dict(enumerate(nets, 1)), x, y, f'Connector_PinHeader_2.54mm:PinHeader_1x0{count}_P2.54mm_Vertical')

    add(bus, 'J1', lib_id, 'HP9830 EDAC 2x25', MAP['connector'], 65, 95, 'Rev2:HP9830_EDAC_2x25')
    add(bus, 'U2', 'Rev2:ATF1504AS_PLCC44', MAP['cpld_part'],
        {p:v[1] for p,v in MAP['cpld'].items()}, 210, 95)
    hct = {1:'HP_INPUT_PL_n', 2:'HP_SPI_SCK', 8:'GND', 15:'GND', 16:'+5V_LOGIC', 7:None}
    lower = dict(hct, **{'10':'GND', '9':'SHIFT_CHAIN'})
    upper = dict(hct, **{'10':'SHIFT_CHAIN', '9':'HP_SPI_MISO'})
    for bit, pin in enumerate([11, 12, 13, 14, 3, 4, 5, 6]):
        lower[pin] = 'DO' + str(bit)
        upper[pin] = ('SO' + str(bit)) if bit < 4 else ('CO' + str(bit-4))
    add(bus, 'U3', '74xx:74HC165', '74HCT165', lower, 115, 200,
        'Package_SO:SOIC-16_3.9x9.9mm_P1.27mm')
    add(bus, 'U4', '74xx:74HC165', '74HCT165', upper, 220, 200,
        'Package_SO:SOIC-16_3.9x9.9mm_P1.27mm')
    header(bus, 'J6', 'CPLD JTAG (5V)', ['+5V_LOGIC','GND','CPLD_TCK','CPLD_TMS','CPLD_TDI','CPLD_TDO'], 345, 175)
    for idx, x in enumerate([290,310,330,350,370,390], 1):
        capacitor(bus, 'C' + str(idx), '100n', '+5V_LOGIC', x, 225)
    for idx, net, rail in [(1,'CPLD_RESET_n','GND'),(2,'CPLD_HP_RUN','GND'),
        (4,'CPLD_CFG_FRAME','GND'),(5,'CPLD_CS_n','+3V3'),(6,'HP_SPI_SCK','+3V3')]:
        resistor(bus, 'R' + str(idx), '10k', net, rail, 50 + (idx-1)*35, 265)
    bus.text('HCT165s and CPLD near J1; CO/SO/DO freeze before REQUEST_n.', 205, 25)
    bus.text('HP outputs: LOW or released. Do not fit additional bus pull-ups.', 205, 31)
    bus.text('HP uses SPI2 PB13/PB14/PB15. PB14 is FT; no MISO level translator.', 205, 37)
    bus.text('Data/status shift directly; CS rising loads ACK/CFI/SSI after data settles.', 205, 43)

    add(mcu, 'U1', 'MCU_ST_STM32F1:STM32F103CBTx', MAP['mcu_part'],
        {p:v[1] for p,v in MAP['mcu'].items()}, 115, 120,
        'Package_QFP:LQFP-48_7x7mm_P0.5mm')
    sd = {1:'SD_DAT2',2:'SD_CS_n',3:'SD_MOSI',4:'+3V3',5:'SD_SCK',6:'GND',7:'SD_MISO',
          8:'SD_DAT1',9:'GND',10:'SD_CD_n','SH':'GND'}
    add(mcu, 'J3', 'Connector:Micro_SD_Card_Det_Hirose_DM3AT', 'microSD + card detect', sd, 305, 85,
        'Connector_Card:microSD_HC_Hirose_DM3AT-SF-PEJM5')
    for idx, net in enumerate(['SD_CS_n','SD_CD_n','SD_DAT1','SD_DAT2'], 7):
        resistor(mcu, 'R'+str(idx), '10k', net, '+3V3', 240+(idx-7)*30, 150)
    header(mcu, 'J4', 'SSD1306 OLED', ['GND','+3V3','OLED_SCL','OLED_SDA'], 350, 190)
    resistor(mcu, 'R11', '4.7k', 'OLED_SCL', '+3V3', 255, 195)
    resistor(mcu, 'R12', '4.7k', 'OLED_SDA', '+3V3', 285, 195)
    for idx, (net, name) in enumerate([('BUTTON_UP_n','UP'),('BUTTON_DOWN_n','DOWN'),('BUTTON_SELECT_n','SELECT')], 1):
        add(mcu, 'SW'+str(idx), 'Switch:SW_Push', name, {1:net,2:'GND'}, 245+(idx-1)*55, 240,
            'Button_Switch_THT:SW_PUSH_6mm_H5mm')
        resistor(mcu, 'R'+str(12+idx), '10k', net, '+3V3', 245+(idx-1)*55, 215)
    header(mcu, 'J5', 'USART1 DEBUG', MAP['debug_header'], 325, 35)
    header(mcu, 'J7', 'SWD', ['+3V3','SWCLK','GND','SWDIO','MCU_RESET_n'], 235, 35)
    header(mcu, 'J8', 'BOOT0 jumper', ['BOOT0','+3V3'], 370, 35)
    resistor(mcu, 'R16', '10k', 'BOOT0', 'GND', 50, 230)
    resistor(mcu, 'R17', '10k', 'BOOT1', 'GND', 80, 230)
    resistor(mcu, 'R18', '10k', 'MCU_RESET_n', '+3V3', 110, 230)
    capacitor(mcu, 'C7', '100n', 'MCU_RESET_n', 140, 230)
    add(mcu, 'SW4', 'Switch:SW_Push', 'RESET', {1:'MCU_RESET_n',2:'GND'}, 170, 230,
        'Button_Switch_THT:SW_PUSH_6mm_H5mm')
    add(mcu, 'Y1', 'Device:Crystal_GND24', '8MHz CL=12pF', {1:'HSE_IN',3:'HSE_OUT',2:'GND',4:'GND'}, 95, 185,
        'Crystal:Crystal_SMD_3225-4Pin_3.2x2.5mm')
    capacitor(mcu, 'C8', '18p', 'HSE_IN', 65, 185)
    capacitor(mcu, 'C9', '18p', 'HSE_OUT', 135, 185)
    for idx, x in enumerate([40,70,100,130,160], 10):
        capacitor(mcu, 'C'+str(idx), '100n', '+3V3', x, 265)
    capacitor(mcu, 'C15', '4.7u', '+3V3', 195, 265)
    mcu.text('USART1: J5 pins 1=3.3V, 2=TX(PA9), 3=RX(PA10), 4=GND.', 205, 15)
    mcu.text('SPI2 HP / SPI1 SD. Disable JTAG, keep SWD for PB4 card detect. FT inputs: no pulls.', 205, 21)

    add(usb, 'J2', 'Rev2:USB_C_HRO_12', 'HRO-TYPE-C-31-M-12',
        {1:'GND',2:'USB_VBUS',3:None,4:'USB_CC1',5:'USB_CONN_DM',6:'USB_CONN_DP',
         7:'USB_CONN_DM',8:'USB_CONN_DP',9:None,10:'USB_CC2',11:'USB_VBUS',12:'GND',13:'GND'},
        70, 85, 'Rev2:HRO-TYPE-C-31-M-12')
    add(usb, 'U5', 'Power_Protection:USBLC6-2SC6', 'USBLC6-2SC6',
        {1:'USB_ESD_DM',2:'GND',3:'USB_ESD_DP',4:'USB_CONN_DP',5:'USB_VBUS',6:'USB_CONN_DM'},
        190, 80, 'Package_TO_SOT_SMD:SOT-23-6')
    resistor(usb, 'R19', '5.1k', 'USB_CC1', 'GND', 50, 140)
    resistor(usb, 'R20', '5.1k', 'USB_CC2', 'GND', 90, 140)
    resistor(usb, 'R21', '22', 'USB_DM', 'USB_ESD_DM', 250, 55)
    resistor(usb, 'R22', '22', 'USB_DP', 'USB_ESD_DP', 290, 55)
    resistor(usb, 'R23', '1.5k', 'USB_PULLUP_EN', 'USB_DP', 340, 55)
    resistor(usb, 'R24', '10k', 'USB_PULLUP_EN', 'GND', 370, 55)
    resistor(usb, 'R25', '1.8k', 'USB_VBUS', 'USB_VBUS_SENSE', 250, 115)
    resistor(usb, 'R26', '3.3k', 'USB_VBUS_SENSE', 'GND', 290, 115)
    add(usb, 'D1', 'Device:D_Schottky', '1N5819WS', {1:'+5V_LOGIC',2:'HP_5V'}, 80, 195, 'Diode_SMD:D_SOD-323')
    add(usb, 'D2', 'Device:D_Schottky', '1N5819WS', {1:'+5V_LOGIC',2:'USB_VBUS'}, 80, 235, 'Diode_SMD:D_SOD-323')
    add(usb, 'U6', 'Regulator_Linear:TLV75533PDRV', 'TLV75533PDRV',
        {1:'+3V3',2:None,3:'GND',4:'+5V_LOGIC',5:None,6:'+5V_LOGIC',7:'GND'},
        205, 210, 'Package_SON:WSON-6-1EP_2x2mm_P0.65mm_EP1x1.6mm')
    capacitor(usb, 'C16', '4.7u', '+5V_LOGIC', 140, 225)
    capacitor(usb, 'C17', '1u', '+3V3', 265, 225)
    capacitor(usb, 'C18', '100n', 'USB_VBUS', 170, 135)
    for idx, net in enumerate(['USB_VBUS','HP_5V','+5V_LOGIC','GND'], 1):
        add(usb, '#FLG'+str(idx), 'power:PWR_FLAG', 'PWR_FLAG', {1:net}, 305+(idx-1)*25, 235)
    usb.text('USB circuit copied from hp21xx_papertape_emulator: USBLC6, 22R, dual 5.1k CC.', 205, 20)
    usb.text('1.5k D+ pull-up driven by PB5; divider VBUS sense uses PA3.', 205, 27)
    usb.text('Use industrial ATF1504AS. Diode-fed 5V rail still requires a worst-case budget.', 205, 165)
    usb.text('Direct FT inputs need power/reset sequencing review before PCB manufacture.', 205, 172)
    for sheet in (bus, mcu, usb): sheet.save()
    (DEST / 'components.json').write_text(json.dumps(sum((s.components for s in (bus,mcu,usb)), []), indent=2) + '\n')
    (DEST / 'MPSI_REV2.kicad_pro').write_text(json.dumps({'meta':{'filename':'MPSI_REV2.kicad_pro','version':1},
        'board':{'design_settings':{'defaults':{'board_thickness':1.6},'rules':{}}}}, indent=2) + '\n')
    print(f'Wrote 3-sheet draft and local connector/USB footprints to {DEST}')


if __name__ == '__main__':
    main()
