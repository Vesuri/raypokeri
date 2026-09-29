#!/usr/bin/env python3
"""Execute the linked opt-in FIFO service against an independent CPU oracle."""
from pathlib import Path
import argparse, re, struct, subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--elf',type=Path,required=True);a=p.parse_args()
names='nativeFifoControlValue nativeShortVideoWriteValue nativeCachedVideoStatus nativeShortPending nativeFeedInlineCount nativeFeedHeaderGrant nativeRasterGrantActive nativeRegisters pendingFrames seenFrames _ZL5board _ZL11videoDevice _ZL10diagnostic _ZL13quitRequested _ZL9liveTicks'.split()
symbols=subprocess.check_output(['m68k-amiga-elf-objdump','-t',str(a.elf)],text=True)
s={v[-1]:int(v[0],16) for line in symbols.splitlines() if (v:=line.split()) and v[-1] in names};assert set(s)==set(names)
for line in symbols.splitlines():
 v=line.split()
 if v and v[-1].endswith('nativeIrqCache'):s['nativeIrqCache']=int(v[0],16)
# Query debug types, never a running target. No native offsets are guessed.
fields={'size':'sizeof(*board)','video':'(unsigned)&((decltype(board))0)->video','fault':'(unsigned)&((decltype(board))0)->fault'}
for name,field in {'ar':'ar','control':'control.values[3]','status':'status','hold':'presentationBusy','error':'error','pending':'pendingCount','read':'readFifo.n'}.items():fields[name]='(unsigned)&((decltype(board))0)->video.'+field
for side in range(2):
 for field in ('control','flags'):fields[f'pia_{field}{side}']=f'(unsigned)&((decltype(board))0)->pia[0].{field}[{side}]'
fields['serial_control']='(unsigned)&((decltype(board))0)->serial[0].control';fields['serial_read']='(unsigned)&((decltype(board))0)->serial[0].receive.n'
args=['m68k-amiga-elf-gdb','-nx','-batch','-ex','file '+str(a.elf)]
for name,expr in fields.items():args+=['-ex',f'printf "LAYOUT {name} %u\\n", {expr}']
out=subprocess.check_output(args,text=True,stderr=subprocess.STDOUT)
layout={k:int(v) for k,v in re.findall(r'LAYOUT (\w+) (\d+)',out)};assert set(layout)==set(fields),out
s.update({'offset_'+k:v for k,v in layout.items()})
b=a.elf.read_bytes();h=struct.unpack_from('>HHIIIIIHHHHHH',b,16);segments=[]
for i in range(h[11]):
 _,kind,flags,address,offset,size,*_=struct.unpack_from('>10I',b,h[5]+i*h[10])
 if kind==1 and flags&2:segments.append(struct.pack('>II',address,size)+b[offset:offset+size])
Path('tmp/fifo-value-code.bin').write_bytes(struct.pack('>I',len(segments))+b''.join(segments));Path('tmp/fifo-value-symbols.txt').write_text(''.join(f'{k} {v}\n' for k,v in s.items()))
subprocess.run(['build/native-fifo-value-test','tmp/fifo-value-code.bin','tmp/fifo-value-symbols.txt'],check=True)
