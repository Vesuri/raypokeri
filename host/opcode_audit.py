#!/usr/bin/env python3
"""Summarize instruction-entry audits; generated ROM observations belong in tmp/.

MC68060UM C.2 lists MOVEP, CHK2/CMP2, CAS2, misaligned CAS and long
multiply/divide forms. Only MOVEP is in the 68000 ISA. Later-ISA or illegal
entries are reported as unknown, never silently declared compatible. This
is an opcode inventory, not proof of all exception/addressing compatibility.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path


def summarize(paths):
    rows = {}
    captures = []
    for path in paths:
        path = Path(path)
        seen = set()
        total = 0
        with path.open(newline='') as source:
            reader = csv.DictReader(source)
            if reader.fieldnames != ['pc', 'opcode', 'entries', 'valid68000', 'needs060isp']:
                raise ValueError(f'{path}: wrong audit schema')
            for row in reader:
                pc, opcode = int(row['pc'], 16), int(row['opcode'], 16)
                count, valid, isp = (int(row[k]) for k in ('entries', 'valid68000', 'needs060isp'))
                key = pc, opcode
                if not (0 <= pc < 0x80000 and pc % 2 == 0 and 0 <= opcode <= 0xffff
                        and count > 0 and valid in (0, 1) and isp in (0, 1)):
                    raise ValueError(f'{path}: invalid observation')
                if key in seen or isp != int(opcode & 0xf138 == 0x0108):
                    raise ValueError(f'{path}: duplicate or inconsistent classification')
                seen.add(key)
                old = rows.setdefault(key, [0, valid, isp])
                if old[1:] != [valid, isp]:
                    raise ValueError(f'{path}: conflicting classification')
                old[0] += count
                total += count
        if not total:
            raise ValueError(f'{path}: empty audit')
        captures.append({'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                         'instruction_entries': total})
    pcs = {}
    for (pc, word), row in rows.items():
        pcs.setdefault(pc, set()).add(word)
    def findings(index, match):
        return [{'pc': f'{pc:05x}', 'entries': row[0]}
                for (pc, _), row in sorted(rows.items()) if row[index] == match]
    return {'captures': captures, 'instruction_entries': sum(r[0] for r in rows.values()),
            'unique_pcs': len(pcs), 'unique_words': len({word for _, word in rows}),
            'ram_pcs': sum(pc >= 0x40000 for pc in pcs),
            'changed_opcode_pcs': [f'{pc:05x}' for pc, words in sorted(pcs.items()) if len(words) > 1],
            'movep': findings(2, 1), 'unknown_68000_entries': findings(1, 0),
            'scope': 'Observed instruction entries only; no unseen-path or full 68060 compatibility claim.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', type=Path, nargs='+')
    args = parser.parse_args()
    print(json.dumps(summarize(args.captures), indent=2))


if __name__ == '__main__':
    main()
