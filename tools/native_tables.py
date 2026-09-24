#!/usr/bin/env python3
"""Generate native offset/operation tables; never emit ROM bytes."""
import csv, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'host'))
from phase3_audit import decode_hook
from relocation_catalog import cpu_control
sys.path.insert(0,str(ROOT/'tools'))
from roms import CHIPS
import hashlib
image=b''
for chip in ['77POK30','77POK38','77POK34','PARA200J']:
    b=(ROOT/'rom'/chip).read_bytes()
    assert (len(b),hashlib.sha256(b).hexdigest())==CHIPS[chip][:2]
    image+=b
rows=list(csv.DictReader((ROOT/'host/tables/io-sites.csv').open()))
sites={int(r['pc'],16):r for r in rows}
hardware_sites=set(sites)
for filename in ['low-vector-hooks.csv','rom-write-hooks.csv']:
    for r in csv.DictReader((ROOT/'host/tables'/filename).open()):
        pc=int(r['pc'],16)
        operation,size,src,dst,length=decode_hook(int.from_bytes(image[pc:pc+2],'big'))
        keys=['source_ea','source_register','source_extension','dest_ea','dest_register','dest_extension']
        sites[pc]=dict(pc=f'{pc:06x}',operation=operation,size=size,length=length,**dict(zip(keys,src+dst)))
lines=['// Generated operation descriptors only. Original operands are read after SHA verification.','#include "native/Hook.h"','using namespace pokeri;','static const pokeri::Hook hooks[]={']
for pc,r in sorted(sites.items()):
    operation={'or':'or_bits','and':'and_bits'}.get(r['operation'],r['operation'])
    def operand(prefix):return '{Ea::%s,%s,%s}'%(r[prefix+'_ea'],r[prefix+'_register'],r[prefix+'_extension'])
    lines.append('{0x%x,%s,%s,Operation::%s,%s,%s},'%(pc,r['length'],r['size'],operation,operand('source'),operand('dest')))
lines+=['};','static const bool hardwareHooks[]={'+','.join('true' if pc in hardware_sites else 'false' for pc in sorted(sites))+'};','struct Fixup {uint32_t offset;unsigned kind;};','static const Fixup fixups[]={']
for r in csv.DictReader((ROOT/'host/tables/relocations.csv').open()):lines.append('{0x%s,%d},'%(r['offset'],['rom','ram','ram_addend','device'].index(r['kind'])))
lines+=['};','struct Access {uint32_t pc,address;unsigned size;bool write;};','static const Access accesses[]={']
for r in sorted(csv.DictReader((ROOT/'host/tables/io-accesses.csv').open()),key=lambda row:int(row['pc'],16)):lines.append('{0x%s,0x%s,%s,%s},'%(r['pc'],r['address'],r['size'],'true' if r['direction']=='W' else 'false'))
lines+=['};','static const uint32_t resets[]={'+','.join('0x'+r['pc'] for r in csv.DictReader((ROOT/'host/tables/reset-hooks.csv').open()))+'};']
# CPU-control instructions are discovered only in covered code, not by scanning data.
lines+=['static const uint32_t controls[]={']
for r in csv.DictReader((ROOT/'host/tables/cpu-control-hooks.csv').open()):
    pc=int(r['pc'],16);word=int.from_bytes(image[pc:pc+2],'big')
    if cpu_control(word)!=r['operation']:raise SystemExit(f'unsupported CPU control at {pc:06x}')
    lines.append('0x%x,'%pc)
lines+=['};']
path=ROOT/'amiga/generated/NativeTables.h';path.parent.mkdir(parents=True,exist_ok=True);path.write_text('\n'.join(lines)+'\n')
print('Native table descriptors:',len(sites),'data-access sites')
