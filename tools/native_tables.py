#!/usr/bin/env python3
"""Generate local native tables and patch guards; output stays in ignored generated/."""
import csv, sys, subprocess, re
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
subprocess.run([str(ROOT/"build/pokeri-host"),"--opcode-cycles"],cwd=ROOT,check=True)
cycles=(ROOT/"tmp/m68000-cycles.bin").read_bytes()
assert len(cycles)==65536
rows=list(csv.DictReader((ROOT/'host/tables/io-sites.csv').open()))
sites={int(r['pc'],16):r for r in rows}
hardware_sites=set(sites)
for filename in ['low-vector-hooks.csv','rom-write-hooks.csv']:
    for r in csv.DictReader((ROOT/'host/tables'/filename).open()):
        pc=int(r['pc'],16)
        operation,size,src,dst,length=decode_hook(int.from_bytes(image[pc:pc+2],'big'))
        keys=['source_ea','source_register','source_extension','dest_ea','dest_register','dest_extension']
        sites[pc]=dict(pc=f'{pc:06x}',operation=operation,size=size,length=length,**dict(zip(keys,src+dst)))
lines=['// Local generated tables and patch guards. Contains ROM-derived values: never commit.','#include "native/Hook.h"','using namespace pokeri;','static const pokeri::Hook hooks[]={']
for pc,r in sorted(sites.items()):
    operation={'or':'or_bits','and':'and_bits'}.get(r['operation'],r['operation'])
    def operand(prefix):return '{Ea::%s,%s,%s}'%(r[prefix+'_ea'],r[prefix+'_register'],r[prefix+'_extension'])
    lines.append('{0x%x,%s,%s,Operation::%s,%s,%s},'%(pc,r['length'],r['size'],operation,operand('source'),operand('dest')))
lines+=['};','static const bool hardwareHooks[]={'+','.join('true' if pc in hardware_sites else 'false' for pc in sorted(sites))+'};','struct Fixup {uint32_t offset;unsigned kind;};','static const Fixup fixups[]={']
for r in csv.DictReader((ROOT/'host/tables/relocations.csv').open()):lines.append('{0x%s,%d},'%(r['offset'],['rom','ram','ram_addend','device'].index(r['kind'])))
lines+=['};','struct Access {uint32_t pc,address;unsigned size;bool write;};','static const Access accesses[]={']
access_rows=sorted(csv.DictReader((ROOT/'host/tables/io-accesses.csv').open()),key=lambda row:int(row['pc'],16))
for r in access_rows:lines.append('{0x%s,0x%s,%s,%s},'%(r['pc'],r['address'],r['size'],'true' if r['direction']=='W' else 'false'))
lines+=['};','struct HookMetadata {uint16_t first,last,cycles;};','static const HookMetadata hookMetadata[]={']
first=0
for pc in sorted(sites):
    while first<len(access_rows) and int(access_rows[first]['pc'],16)<pc:first+=1
    last=first
    while last<len(access_rows) and int(access_rows[last]['pc'],16)==pc:last+=1
    lines.append('{%d,%d,%d},'%(first,last,cycles[int.from_bytes(image[pc:pc+2],'big')]))
    first=last
assert first==len(access_rows) and first<65536
lines+=['};','static const uint32_t resets[]={'+','.join('0x'+r['pc'] for r in csv.DictReader((ROOT/'host/tables/reset-hooks.csv').open()))+'};']
# CPU-control instructions are discovered only in covered code, not by scanning data.
lines+=['static const uint32_t controls[]={']
for r in csv.DictReader((ROOT/'host/tables/cpu-control-hooks.csv').open()):
    pc=int(r['pc'],16);word=int.from_bytes(image[pc:pc+2],'big')
    if cpu_control(word)!=r['operation']:raise SystemExit(f'unsupported CPU control at {pc:06x}')
    lines.append('0x%x,'%pc)
lines+=['};']
# Check only the bytes the native loader will patch, not a whole-image hash.
# These ROM-derived constants remain exclusively in the ignored generated header.
patch_words={offset for pc,r in sites.items() for offset in range(pc,pc+int(r["length"]),2)} | {0x10ae,0x10b0,0x110c,0x110e,0x2194}
# Approved feed-loop fusion: guard all comparisons, branches and ring-wrap load.
patch_words.update(range(0x2e54,0x2e70,2))
# Check the authored address metadata against the user's ROM, including short
# and word-displacement BSR forms. Keep the original bytes local-only.
assert image[0x1e0c2:0x1e0c4] == bytes.fromhex("4e75"), "shuffle boundary is not RTS"
shuffle_callers=[int(x,16) for x in re.findall(r"case (0x[0-9a-f]+):",(ROOT/'src/native/ShuffleWait.h').read_text())]
assert len(set(shuffle_callers)) == 15
for target in shuffle_callers:
    short=image[target-2]==0x61 and image[target-1]!=0 and target+int.from_bytes(image[target-1:target],"big",signed=True)==0x1e08a
    long=image[target-4:target-2]==bytes.fromhex("6100") and target-2+int.from_bytes(image[target-2:target],"big",signed=True)==0x1e08a
    assert short or long, f"shuffle caller guard mismatch {target:x}"
# Shuffle boundary and callers: guard the complete original helper/loop.
patch_words.update(range(0x1dfa0,0x1e0c4,2))
# Authorized idle experiment: guard both the decrement and its short branch.
patch_words.update(range(0x2442,0x2446,2))
# User-approved fast boot patch spans; guard original bytes, emitted only here.
# The RAM patch changes a PC-relative LEA destination; other entries replace
# a four-byte prologue with newly assembled MOVEQ/RTS.
for begin,end in [(0x121c,0x1220),(0x134a,0x1350),(0x1358,0x135e),(0x1380,0x1386),(0x1f2e,0x1f32),(0x25a4,0x25aa),(0x5b9c,0x5ba0),
                  (0x10f2c,0x10f30),(0x16e1c,0x16e20)]:
    patch_words.update(range(begin,end,2))
# Native short-loop timing depends on these entire branch/decrement paths.
# Guard their bytes as well as the replaced BTST opcode, without patching them.
for begin,end in [(0x20be,0x20cc),(0x2118,0x2122),(0x214a,0x2156)]:
    patch_words.update(range(begin,end,2))
# SR-immediate controls bake their full operand into the prepared descriptor.
for r in csv.DictReader((ROOT/'host/tables/cpu-control-hooks.csv').open()):
    if r['operation'] in ('and_sr','or_sr','eor_sr'):patch_words.add(int(r['pc'],16)+2)
for filename in ['reset-hooks.csv','cpu-control-hooks.csv']:
    patch_words.update(int(r['pc'],16) for r in csv.DictReader((ROOT/'host/tables'/filename).open()))
for r in csv.DictReader((ROOT/'host/tables/relocations.csv').open()):
    offset=int(r['offset'],16);patch_words.update([offset,offset+2])
lines+=['struct PatchWord {uint32_t offset;uint16_t value;};','static const PatchWord patchWords[]={']
for offset in sorted(patch_words):
    lines.append('{0x%x,0x%x},'%(offset,int.from_bytes(image[offset:offset+2],'big')))
lines+=['};']
# T13 setup's complete original bytes stay local and compile out by default.
lines += ['#ifdef POKERI_HANDLER_SETUP_FUSION', 'static const PatchWord handlerSetupWords[]={']
for offset in range(0x2e36,0x2e58,2):
    lines.append('{0x%x,0x%x},'%(offset,int.from_bytes(image[offset:offset+2],'big')))
lines += ['};','#endif']
# Nominal original 68000 timing, metadata only; no emulator is linked on Amiga.
lines+=['struct HookCycles {uint32_t pc;uint16_t cycles;};','static const HookCycles originalCycles[]={']
for offset in sorted(set(sites) | set(int(r['pc'],16) for name in ['reset-hooks.csv','cpu-control-hooks.csv'] for r in csv.DictReader((ROOT/'host/tables'/name).open()))):
    lines.append('{0x%x,%d},'%(offset,cycles[int.from_bytes(image[offset:offset+2],'big')]))
lines+=['};']
path=ROOT/'amiga/generated/NativeTables.h';path.parent.mkdir(parents=True,exist_ok=True);path.write_text('\n'.join(lines)+'\n')
print('Native table descriptors:',len(sites),'data-access sites')
