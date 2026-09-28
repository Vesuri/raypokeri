#!/usr/bin/env python3
"""Original-code cold/warm accounting checks; all captures remain in tmp/."""
from pathlib import Path
import shutil
import struct
import subprocess
import zlib

ROOT=Path(__file__).resolve().parents[1]
COMMON=['build/pokeri-host','--devices','--skip-hardware-tests','--serial-peer',
        '--system-hz','100','--input-hz','50','--watchdog-ms','400',
        '--watchdog-reset-us','50000','--ay-clock','1000000','--stall-instructions','0']
PREFIX='tmp/accounting-check-'

def run(name,extra):
    prefix=PREFIX+name
    with (ROOT/(prefix+'.log')).open('w') as log:
        subprocess.run(COMMON+['--out',prefix]+extra,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
    return (ROOT/(prefix+'-ram.bin')).read_bytes()

def encode(ram,path):
    data=b'PKAC0001'+ram[0x3e60:0x4200]
    (ROOT/path).write_bytes(data+struct.pack('>I',zlib.crc32(data)))

def accounting(ram):return ram[0x3e60:0x4200]
def balance(ram,address):return int.from_bytes(ram[address-0x40000:address-0x40000+4],'big')
def ready(ram):return [ram[0x8b00-d] for d in (0x770c,0x78ce,0x78de,0x78d2)]==[0,0,1,0]

def main():
    seed=PREFIX+'seed.bin';(ROOT/seed).unlink(missing_ok=True)
    cold=run('cold',['--auto-setup','--accounting-ram',seed,'--ms','1000'])
    assert ready(cold) and balance(cold,0x4400c)==100 and balance(cold,0x44074)==0
    warm=[]
    for name,bases in [('ref',None),('a',(0x100000,0x200000,0x300000)),('b',(0x512300,0x684680,0x923400))]:
        path=PREFIX+name+'.bin';shutil.copyfile(ROOT/seed,ROOT/path)
        extra=['--auto-setup','--accounting-ram',path,'--ms','1000']
        if bases:
            extra+=['--bypass-module-checksums','--io-table','host/tables/io-accesses.csv']
            for flag,value in zip(('--rom-base','--ram-base','--device-base'),bases):extra += [flag,hex(value)]
        else:extra+=['--save-state',PREFIX+'warm.state']
        ram=run('warm-'+name,extra);assert ready(ram);warm.append(accounting(ram))
        log=(ROOT/(PREFIX+'warm-'+name+'-events.txt')).read_text()
        assert 'setup stage=4 ' not in log,'warm boot entered refill'
    assert warm[0]==warm[1]==warm[2],'accounting differs across placements'
    print('PASS cold/warm: zero player credits, retained reserve, no warm refill, two relocation placements',flush=True)

    # Generate nonzero credit and in-progress hand fixtures by original code.
    # The warm snapshot is already at ~2.69 s; all later inputs are external.
    inputs=PREFIX+'play.inputs'
    (ROOT/inputs).write_text('3000 packet 3 0\n3500 1 0 0xfe\n3700 1 0 0xff\n')
    for name,end in [('credit',3400),('hand',6000)]:
        source=run('source-'+name,['--load-state',PREFIX+'warm.state','--inputs',inputs,'--ms',str(end)])
        if name=='credit':assert balance(source,0x44074)>0
        narrow=PREFIX+name+'.bin';encode(source,narrow)
        # Verify selected retention against retaining the entire physical RAM.
        # Both boot the original CPU and receive the same cabinet pin/messages.
        full=PREFIX+name+'-full.bin';(ROOT/full).write_bytes(source)
        pins=PREFIX+'warm.inputs'
        (ROOT/pins).write_text('0 1 0 0xff\n0 1 1 0x7f\n0 2 0 8\n3000 packet 1 0x20000\n3000 packet 0x31 0x20100\n')
        common=['--inputs',pins,'--ms','8000']
        a=run('recover-'+name,['--accounting-ram',narrow]+common)
        b=run('recover-'+name+'-full',['--retained-ram',full]+common)
        assert ready(a) and ready(b)
        # Full RAM also retains the runtime restart marker. Original reset at
        # $21E6 logs event 7 through $D164, updating only the two-slot event
        # history ($06/07, $0E/0F, $28..2F of context). A fresh power-on has no
        # such marker. Compare every other retained byte, including all three
        # checksummed accounting copies; do not mask game/accounting state.
        history={0x3e66,0x3e67,0x3e6e,0x3e6f,*range(0x3e88,0x3e90)}
        differences={i for i in range(0x3e60,0x4200) if a[i]!=b[i]}
        assert differences<=history,'unexplained retained recovery difference: '+name
        assert b[0x3e66]==7 or b[0x3e67]==7,'full RAM did not log expected runtime restart'

        if name=='credit':assert balance(a,0x44074)==balance(source,0x44074)
        print('PASS accounting/recovery matches full RAM except its evidenced restart event: '+name,flush=True)
    print('PASS retained accounting checks; no SDL launched',flush=True)

if __name__=='__main__':main()
