#!/usr/bin/env python3
"""Inspect Hilpert XNS images and reproduce an experimental BOF-to-TAP encoding.

This is an analysis tool, not an STM32 backend or a general SIMH converter.
The raw-cell experiment does not preserve conventional TAP-file semantics;
see requirements/07_tape_analysis.md and open issue 009.
It mirrors software/util/util.c's minimal XNS decoder. No input is modified.
"""

import argparse
import hashlib
import json
import struct
from pathlib import Path


BOF = 0x083C
FIELDS = ("number", "length_words", "type", "space_words", "first_line",
          "last_line", "common_words")


def decode_xns(text):
    cells, value = [], 0
    for char in text:
        if char in "0123456789ABCDEF":
            value = (value << 4) | int(char, 16)
        elif char == "~":
            cells.append(value)
            value = 0
        elif char == "!":
            break
        else:
            value = 0
    return cells


def word(cells, offset):
    if offset + 2 > len(cells):
        raise ValueError(f"truncated word at cell {offset}")
    low, high = cells[offset:offset + 2]
    if not (0 <= low <= 255 and 0 <= high <= 255):
        raise ValueError(f"non-data cell inside word at {offset}")
    return low | (high << 8)


def checksum_result(stored, calculated):
    return {"stored": stored, "calculated": calculated,
            "matches": stored == calculated}


def inspect_t98(cells):
    files, position, previous_end = [], 0, 0
    while position < len(cells):
        if cells[position] == 0:
            position += 1
            continue
        if cells[position] not in (0x3C, BOF):
            raise ValueError(f"unexpected cell outside a file at {position}: "
                             f"0x{cells[position]:X}")
        header = [word(cells, position + 1 + i * 2) for i in range(13)]
        entry = dict(zip(FIELDS, header))
        length, space = header[1], header[3]
        if length > space:
            raise ValueError(f"file length exceeds allocated space at {position}")
        content = position + 27
        end = content + (space + 1) * 2
        if end > len(cells):
            raise ValueError(f"truncated allocated file at {position}")
        # Check every reserved word too, rather than overlooking control flags.
        allocated = [word(cells, content + i * 2) for i in range(space + 1)]
        entry.update({
            "bof_cell": position,
            "control_marked": cells[position] == BOF,
            "padding_before": position - previous_end,
            "content_cell": content,
            "allocated_end_cell": end,
            "header_words": header[:12],
            "header_checksum": checksum_result(header[12], sum(header[:12]) & 0xFFFF),
            "content_checksum": checksum_result(allocated[length], sum(allocated[:length]) & 0xFFFF),
        })
        files.append(entry)
        previous_end = end
        position = end
    return {"cell_count": len(cells), "files": files,
            "trailing_padding": len(cells) - previous_end,
            "marked_bof_cells": [i for i, c in enumerate(cells) if c == BOF],
            "ordinary_3c_count": cells.count(0x3C)}


def inspect_f98(words):
    if len(words) < 14 or any(w < 0 or w > 0xFFFF for w in words):
        raise ValueError("invalid or incomplete word-oriented .f98 image")
    length = words[1]
    if 13 + length >= len(words):
        raise ValueError("truncated .f98 contents/checksum")
    return {"word_count": len(words), "header_words": words[:12],
            "header_checksum": checksum_result(words[12], sum(words[:12]) & 0xFFFF),
            "content_checksum": checksum_result(words[13 + length], sum(words[13:13 + length]) & 0xFFFF)}


def proposed_tap(cells):
    """Superseded raw-cell experiment: byte runs become records, BOFs marks."""
    image, objects, pending = bytearray(), [], bytearray()

    def flush():
        if not pending:
            return
        length = len(pending)
        if length > 0xFFFFFF:
            raise ValueError("record exceeds standard SIMH length limit")
        objects.append({"record_bytes": length})
        image.extend(struct.pack("<I", length))
        image.extend(pending)
        if length & 1:
            image.append(0)  # Container alignment, not a cassette byte.
        image.extend(struct.pack("<I", length))
        pending.clear()

    for cell in cells:
        if cell == BOF:
            flush()
            image.extend(b"\0\0\0\0")
            objects.append({"tape_mark": True})
        elif 0 <= cell <= 255:
            pending.append(cell)
        else:
            raise ValueError(f"mapping does not support control cell 0x{cell:X}")
    flush()
    return bytes(image), objects


def decode_proposed_tap(image, reverse=False):
    """Read normal records/marks in both directions, validating their framing."""
    cells, position = [], len(image) if reverse else 0
    while position > 0 if reverse else position < len(image):
        tag_at = position - 4 if reverse else position
        if tag_at < 0 or tag_at + 4 > len(image):
            raise ValueError("truncated TAP framing")
        length = struct.unpack_from("<I", image, tag_at)[0]
        if length == 0:
            cells.append(BOF)
            position += -4 if reverse else 4
            continue
        if length > 0xFFFFFF:
            raise ValueError("unsupported TAP marker or record class")
        start = position - 8 - length - (length & 1) if reverse else position
        stop = start + 8 + length + (length & 1)
        if start < 0 or stop > len(image):
            raise ValueError("truncated TAP record")
        head = struct.unpack_from("<I", image, start)[0]
        tail = struct.unpack_from("<I", image, stop - 4)[0]
        if head != length or tail != length:
            raise ValueError("TAP record lengths disagree")
        data = image[start + 4:start + 4 + length]
        cells.extend(reversed(data) if reverse else data)
        position = start if reverse else stop
    return cells


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--verify-tap", action="store_true",
                        help="check superseded raw-cell experiment and lost-BOF recovery")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    reports, images, source_files = [], [], []
    for path in args.paths:
        raw = path.read_bytes()
        cells = decode_xns(raw.decode("ascii"))
        report = {"path": str(path), "file_bytes": len(raw),
                  "sha256": hashlib.sha256(raw).hexdigest()}
        if path.suffix.lower() == ".t98":
            report.update(inspect_t98(cells))
            images.append((report, cells))
            if args.verify_tap:
                tap, objects = proposed_tap(cells)
                forward = decode_proposed_tap(tap) == cells
                reverse = decode_proposed_tap(tap, reverse=True) == cells[::-1]
                stripped = [c & 0xFF for c in cells]
                recovered = stripped[:]
                for entry in inspect_t98(stripped)["files"]:
                    recovered[entry["bof_cell"]] = BOF
                if not (forward and reverse and recovered == cells):
                    raise ValueError("round-trip or BOF recovery failed")
                report["proposed_tap"] = {"status": "superseded_raw_cell_experiment",
                                          "file_bytes": len(tap), "objects": objects,
                                          "forward_cells_equal": forward,
                                          "reverse_cells_equal": reverse,
                                          "stripped_control_flags_recovered": True}
        elif path.suffix.lower() == ".f98":
            report.update(inspect_f98(cells))
            source_files.append((path, cells))
        else:
            parser.error(f"expected .t98 or .f98: {path}")
        reports.append(report)
    for report, cells in images:
        for entry in report["files"]:
            start, length = entry["content_cell"], entry["length_words"]
            content = [word(cells, start + i * 2) for i in range(length)]
            entry["matching_f98_content"] = [str(p) for p, w in source_files
                                             if w[1] == length and w[13:13 + length] == content]
    if args.json:
        print(json.dumps(reports, indent=2))
    else:
        for report in reports:
            print(json.dumps(report))


if __name__ == "__main__":
    main()
