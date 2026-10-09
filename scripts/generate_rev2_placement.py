#!/usr/bin/env python3
"""Create an unrouted placement draft using KiCad's bundled Python/pcbnew.

The legacy board is used only for its connector and complete Edge.Cuts outline.
The draft is disposable; netlist and board links point to the Rev2 schematic.
"""
import json
import argparse
from pathlib import Path
import pcbnew as pcb
from kicad_sexpr import read, child, children

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'hardware/rev2/kicad'
PINMAP = json.loads((ROOT / 'hardware/rev2/pinmap.json').read_text())
PARTS = json.loads((PROJECT / 'components.json').read_text())
LIBS = Path('/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints')


def pos(x,y):
    return pcb.VECTOR2I(pcb.FromMM(x),pcb.FromMM(y))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--netlist', type=Path, default=Path('/tmp/mpsi-rev2.net'))
    parser.add_argument('--footprints', type=Path, default=LIBS)
    args = parser.parse_args()
    board = pcb.BOARD()
    legacy = pcb.LoadBoard(str(ROOT / 'MPSIBOARD/MPSIBOARD.kicad_pcb'))
    for graphic in legacy.GetDrawings():
        if graphic.GetLayer() == pcb.Edge_Cuts:
            board.Add(graphic.Duplicate())
    connector = next(f for f in legacy.GetFootprints() if f.GetReference() == 'J1')
    netlist = read(args.netlist)
    pad_nets = {}
    for net in children(child(netlist, 'nets'), 'net'):
        for node in children(net, 'node'):
            pad_nets[child(node, 'ref')[1],child(node, 'pin')[1]] = child(net, 'name')[1]
    nets = {}
    for name in sorted(set(pad_nets.values())):
        net = pcb.NETINFO_ITEM(board,name)
        board.Add(net); nets[name] = net
    places = dict(PINMAP['placement_mm'])
    places.update({
      'U5':[155,116,90], 'U6':[131,109,0], 'J6':[92,112,90], 'J7':[127,55,0], 'J8':[138,58,0],
      'SW1':[145,57,0], 'SW2':[157,57,0], 'SW3':[169,57,0], 'SW4':[119,98,0], 'Y1':[106,85,0],
      'C1':[72,66,0], 'C2':[93,66,0], 'C3':[93,90,0], 'C4':[72,90,0],
      'C5':[63,102,0], 'C6':[63,67,0], 'C7':[110,102,0], 'C8':[105,81,0], 'C9':[105,90,0],
      'C10':[112,72,0], 'C11':[124,73,0], 'C12':[124,87,0], 'C13':[112,88,0],
      'C14':[110,70,0], 'C15':[118,93,0], 'C16':[125,108,0], 'C17':[134,113,0], 'C18':[163,107,0],
      'D1':[129,104,0], 'D2':[129,106,0],
      'R1':[101,71,0], 'R2':[101,73,0], 'R3':[109,77,0], 'R4':[109,79,0],
      'R5':[101,79,0], 'R6':[101,81,0], 'R7':[145,73,0], 'R8':[145,76,0],
      'R9':[145,79,0], 'R10':[145,82,0], 'R11':[136,44,0], 'R12':[136,47,0],
      'R13':[145,64,0], 'R14':[157,64,0], 'R15':[169,64,0], 'R16':[138,66,0],
      'R17':[117,68,0], 'R18':[110,100,0], 'R19':[167,101,0], 'R20':[167,104,0],
      'R21':[148,115,0], 'R22':[148,118,0], 'R23':[148,107,0], 'R24':[145,107,0],
      'R25':[157,104,0], 'R26':[157,107,0]
    })
    for part in PARTS:
        if not part['footprint'] or part['reference'].startswith('#'):
            continue
        ref = part['reference']
        nickname,name = part['footprint'].split(':',1)
        directory = PROJECT / 'Rev2.pretty' if nickname=='Rev2' else args.footprints / (nickname + '.pretty')
        fp = pcb.FootprintLoad(str(directory),name)
        if fp is None:
            raise RuntimeError('missing footprint '+part['footprint'])
        if ref == 'J1':
            fp.SetPosition(connector.GetPosition()); fp.SetOrientation(connector.GetOrientation())
        else:
            x,y,rotation = places[ref]
            fp.SetPosition(pos(x,y)); fp.SetOrientationDegrees(rotation)
        fp.SetReference(ref); fp.SetValue(part['value'])
        if ref[0] in 'RCD':
            label = fp.Reference()
            label.SetTextSize(pos(0.8,0.8)); label.SetTextThickness(pcb.FromMM(0.12))
            label.SetPosition(pos(x - (4 if ref[0]=='D' else 3), y))
            # A few crowded references fit above/below their part instead.
            if ref in {'C8','D1','R23'}:
                label.SetPosition(pos(x + 3,y) if ref=='C8' else pos(x,y - 2))
        for field_name in ('Datasheet','Description'):
            field = fp.GetField(field_name)
            if field: field.SetText('')
        fp.SetFPID(pcb.LIB_ID(nickname,name))
        fp.SetPath(pcb.KIID_PATH(part['path'] + '/' + part['uuid']))
        board.Add(fp)
        for pad in fp.Pads():
            net = pad_nets.get((ref,pad.GetNumber()))
            pad.SetNet(nets[net] if net else board.FindNet(0))
        if ref == 'J1':
            original = {p.GetNumber():p for p in connector.Pads()}
            for pad in fp.Pads():
                prior = original[pad.GetNumber()]
                assert pad.GetPosition() == prior.GetPosition(), 'connector pad moved'
                assert pad.GetSize() == prior.GetSize(), 'connector pad size changed'
                assert abs((pad.GetOrientationDegrees() - prior.GetOrientationDegrees() + 90) % 180 - 90) < 1e-6, \
                    'connector pad rotated: '+pad.GetNumber()+': '+str(pad.GetOrientationDegrees())+' vs '+str(prior.GetOrientationDegrees())
                assert pad.GetLayerSet().FmtBin() == prior.GetLayerSet().FmtBin(), 'connector pad side changed'
    board.GetDesignSettings().SetCopperLayerCount(2)
    pcb.SaveBoard(str(PROJECT / 'MPSI_REV2.kicad_pcb'),board)
    print(f'Wrote unrouted placement draft: copied connector/outline, {len(board.GetFootprints())} footprints, two copper layers')


if __name__ == '__main__':
    main()
