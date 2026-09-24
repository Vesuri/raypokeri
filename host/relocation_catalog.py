#!/usr/bin/env python3
"""Reproduce byte-free Phase 3 coverage, reset, ROM-write and abs.l metadata.

The assembly listing is research output in tmp/ only. No translated game code
or original bytes enter the generated catalog.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from roms import CHIPS


def cpu_control(word):
    named={0x4e73:"rte",0x007c:"or_sr",0x027c:"and_sr",0x0a7c:"xor_sr"}
    if word in named:return named[word]
    if word&0xfff0==0x4e60:return "move_usp"
    if word&0xffc0==0x40c0:return "read_sr"
    if word&0xffc0==0x46c0:return "write_sr"
    return None


def catalog(out):
    out.mkdir(parents=True,exist_ok=True)
    image=bytearray()
    for name in ['77POK30','77POK38','77POK34','PARA200J']:
        payload=(ROOT/'rom'/name).read_bytes();size,digest,_=CHIPS[name]
        if len(payload)!=size or hashlib.sha256(payload).hexdigest()!=digest:raise ValueError('ROM mismatch: '+name)
        image.extend(payload)
    coverage=(ROOT/'tmp/relocation-union-coverage.bin').read_bytes()
    if len(coverage)!=0x20000:raise ValueError('invalid coverage size')
    subprocess.run(['build/pokeri-host','--code-map','tmp/relocation-union-coverage.bin','--out','tmp/relocation-static'],cwd=ROOT,check=True)
    pcs=[p for p in range(0x100000) if coverage[p>>3]&(1<<(p&7))]
    writes=set()
    for name in ['setup','attract','deal','win','double','service']:
        with (ROOT/f'tmp/relocation-reference-{name}-rom-writes.csv').open() as f:
            for row in csv.DictReader(f):writes.add(tuple(row[k] for k in ['pc','address','size','direction']))
    def csv_file(name,fields,rows):
        with (out/name).open('w') as f:
            writer=csv.writer(f,lineterminator='\n');writer.writerow(fields);writer.writerows(rows)
    csv_file('rom-write-hooks.csv',['pc','address','size','direction'],sorted(writes))
    resets=[p for p in pcs if p<0x40000 and int.from_bytes(image[p:p+2],'big')==0x4e70]
    csv_file('reset-hooks.csv',['pc','operation'],[(f'{p:06x}','peripheral_reset') for p in resets])
    controls=[(f'{p:06x}',cpu_control(int.from_bytes(image[p:p+2],'big'))) for p in pcs if p<0x40000]
    csv_file('cpu-control-hooks.csv',['pc','operation'],[(p,op) for p,op in controls if op])
    with (ROOT/'host/tables/relocations.csv').open() as f:relocs={int(r['offset'],16):r['kind'] for r in csv.DictReader(f)}
    with (ROOT/'tmp/relocation-static-code.csv').open() as f:lengths={int(r['pc'],16):int(r['length']) for r in csv.DictReader(f)}
    absolute=set()
    for line in (ROOT/'tmp/relocation-static-code-asm.txt').read_text().splitlines():
        pc=int(line[:6],16)
        for match in re.finditer(r'(?<!#)\$([0-9a-f]+)\.l\b',line):
            value=int(match[1],16)
            offsets=[o for o in range(pc+2,pc+lengths[pc]-3,2) if int.from_bytes(image[o:o+4],'big')==value]
            if not offsets:raise ValueError(f'absolute operand not decoded at {pc:06x}')
            for offset in offsets:
                if offset not in relocs:raise ValueError(f'covered absolute operand missing relocation: {offset:06x}')
                absolute.add((f'{pc:06x}',offset-pc,relocs[offset]))
    csv_file('covered-absolute.csv',['pc','operand_offset','kind'],sorted(absolute))
    summary={'profile':'checksum bypass; relocation-play and service scenarios','covered_pcs':len(pcs),
             'rom_pcs':sum(p<0x40000 for p in pcs),'ram_pcs':sum(0x40000<=p<0x80000 for p in pcs),
             'union_sha256':hashlib.sha256(coverage).hexdigest(),'relocation_records':len(relocs),
             'covered_absolute_operands':len(absolute),'covered_reset_sites':len(resets),'low_vector_hooks':5,
             'rom_write_sites':len(writes),'uncovered':['other service pages and accounting configurations',
             'payout/hopper firmware and alternate serial transactions','DUART and 2 MB video/RAMDAC path',
             'ROM-writable development mode','code and data paths outside the scenario coverage union']}
    (out/'coverage.json').write_text(json.dumps(summary,indent=2)+'\n')
    return summary


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--out',type=Path,default=ROOT/'tmp/relocation-catalog')
    args=parser.parse_args();print(json.dumps(catalog(args.out),indent=2))
