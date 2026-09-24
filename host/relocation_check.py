#!/usr/bin/env python3
"""Phase 3: original code, two placements, explicit hooks, byte/trace equality."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'host'))
from scenarios.check import COMMON

PLACEMENTS = {'reference': (0,0x40000,0x80000), 'a': (0x100000,0x200000,0x300000),
              'b': (0x512300,0x684680,0x923400)}
MILESTONES = [('setup',25000,None,'relocation-play'),('attract',40500,'setup','relocation-play'),
              ('deal',47500,'attract','relocation-play'),('win',60000,'deal','relocation-play'),
              ('double',69500,'win','relocation-play'),('service',39000,'setup','service')]


def prefix(profile, name):
    return f'tmp/relocation-{profile}-{name}'


def data(profile, name, suffix):
    return (ROOT / (prefix(profile,name)+suffix)).read_bytes()


def command(profile):
    args=['build/pokeri-host',*COMMON,'--bypass-module-checksums','--io-table','host/tables/io-accesses.csv','--ram-provenance','host/tables/ram-provenance-sites.txt']
    if profile!='reference':
        for opt,value in zip(['--rom-base','--ram-base','--device-base'],PLACEMENTS[profile]):
            args += [opt,hex(value)]
    return args


def ram_comparison(name):
    ref=data('reference',name,'-ram.bin');a=data('a',name,'-ram.bin');b=data('b',name,'-ram.bin')
    covered=set();fixups=[]
    # Differences must agree with a placement delta in BOTH relocated runs and
    # the unrelocated reference. Numeric values that merely resemble addresses
    # are never independently normalized by a range heuristic.
    for i in range(0,len(ref)-3,2):
        r=int.from_bytes(ref[i:i+4],'big');x=int.from_bytes(a[i:i+4],'big');y=int.from_bytes(b[i:i+4],'big')
        if r==x==y:continue
        for kind,j in [('rom',0),('ram',1),('device',2)]:
            da=PLACEMENTS['a'][j]-PLACEMENTS['reference'][j];db=PLACEMENTS['b'][j]-PLACEMENTS['reference'][j]
            if x==(r+da)&0xffffffff and y==(r+db)&0xffffffff:
                fixups.append([f'{i+0x40000:06x}',kind]);covered.update(range(i,i+4));break
    misses=[i for i,(r,x,y) in enumerate(zip(ref,a,b)) if not r==x==y and i not in covered]
    if misses:
        witnesses=[]
        for profile in PLACEMENTS:
            with (ROOT/(prefix(profile,name)+'-ram-writers.csv')).open() as f:
                witnesses.append({int(row['byte'],16):row for row in csv.DictReader(f)})
        for i in misses[:]:
            records=[w.get(i+0x40000) for w in witnesses]
            if any(r is None for r in records):continue
            first=records[0]
            if first['size']!='4' or any(any(r[k]!=first[k] for k in ['pc','address','size','instruction']) for r in records):continue
            values=[int(r['value'],16) for r in records]
            lane=i+0x40000-int(first['address'],16)
            if not 0<=lane<4:continue
            if any(v.to_bytes(4,'big')[lane]!=buf[i] for v,buf in zip(values,[ref,a,b])):continue
            for kind,j in [('rom',0),('ram',1),('device',2)]:
                if all(v==(values[0]+PLACEMENTS[profile][j]-PLACEMENTS['reference'][j])&0xffffffff for v,profile in zip(values,PLACEMENTS)):
                    fixups.append([f'{i+0x40000:06x}',kind+'_fragment_from_'+first['pc']]);misses.remove(i);break
    if misses:
        for i in misses[:12]:print(f'RAM mismatch {i+0x40000:06x}: {ref[i:i+8].hex()} / {a[i:i+8].hex()} / {b[i:i+8].hex()}',flush=True)
        raise AssertionError(f'{name}: {len(misses)} RAM bytes not explained by two-base relocation')
    return fixups


def traces(name):
    files=[(ROOT/(prefix(p,name)+'-trace.csv')).open() for p in PLACEMENTS]
    try:
        from itertools import zip_longest
        for number,rows in enumerate(zip_longest(*(csv.DictReader(f) for f in files)),1):
            if any(r is None for r in rows):raise AssertionError(f'{name}: trace length mismatch')
            original=rows[0]
            for profile,row in zip(PLACEMENTS,rows):
                if {k:v for k,v in row.items() if k!='cpu_address'}!={k:v for k,v in original.items() if k!='cpu_address'}:
                    raise AssertionError(f'{name}: trace mismatch row {number}')
                expected=int(row['address'],16)+PLACEMENTS[profile][2]-0x80000
                if int(row['cpu_address'],16)!=expected:raise AssertionError('device address not relocated by guard delta')
    finally:
        for f in files:f.close()


def compare(name):
    for suffix in ['-coverage.bin','-nvram.bin','-devices.txt','-final.ppm']+(['.wav'] if name=='double' else []):
        expected=data('reference',name,suffix)
        for p in ['a','b']:
            if data(p,name,suffix)!=expected:raise AssertionError(f'{name}: {p} differs in {suffix}')
    # Complete portable device state, excluding only the RAM bytes checked by
    # the three-way relocation/provenance comparison below.
    boards=[data(p,name,'-board-state.bin') for p in PLACEMENTS]
    config_bytes=5*4
    for image in boards[1:]:
        if image[:config_bytes]!=boards[0][:config_bytes] or image[config_bytes+0x40000:]!=boards[0][config_bytes+0x40000:]:
            raise AssertionError(f'{name}: full device state mismatch')
    cpus=[data(p,name,'-cpu-state.bin') for p in PLACEMENTS]
    if len({len(c) for c in cpus})!=1 or len(cpus[0])%4:raise AssertionError('CPU context size mismatch')
    for i in range(0,len(cpus[0]),4):
        values=[int.from_bytes(c[i:i+4],sys.byteorder) for c in cpus]
        if values[0]==values[1]==values[2]:continue
        if not any(all(v==(values[0]+PLACEMENTS[profile][j]-PLACEMENTS['reference'][j])&0xffffffff for v,profile in zip(values,PLACEMENTS)) for j in range(3)):
            raise AssertionError(f'{name}: full CPU context mismatch at byte {i}: {values}')
    # Registers and current PC are pointers or ordinary unchanged integers;
    # SR, instruction and cycle counts must match exactly.
    contexts=[data(p,name,'-context.txt').decode() for p in PLACEMENTS]
    fields=[dict(re.findall(r'(instructions|cycles|pc|sr|[DA][0-7])=([0-9a-f]+)',c.split('\n',6)[0]+'\n'+'\n'.join(c.splitlines()[1:6]))) for c in contexts]
    for key in fields[0]:
        values=[int(f[key],10 if key in ('instructions','cycles') else 16) for f in fields]
        if values[0]==values[1]==values[2]:continue
        if key in ('instructions','cycles','sr'):raise AssertionError(f'{name}: CPU {key} differs')
        if not any(all(values[i]-values[0]==PLACEMENTS[p][j]-PLACEMENTS['reference'][j] for i,p in enumerate(PLACEMENTS)) for j in range(3)):
            raise AssertionError(f'{name}: unexplained register {key}: {values}')
    fixups=ram_comparison(name);traces(name)
    print(f'PASS {name}: CPU, RAM ({len(fixups)} rebased fields), coverage, frame, devices and bus trace',flush=True)
    return fixups


def replay_check():
    for suffix in ['-ram.bin','-coverage.bin','-nvram.bin','-devices.txt','-final.ppm','-board-state.bin','-cpu-state.bin','.state']:
        if (ROOT/('tmp/relocation-b-uninterrupted'+suffix)).read_bytes()!=data('b','double',suffix):
            raise AssertionError('relocated snapshot replay differs: '+suffix)
    import wave
    with wave.open(str(ROOT/'tmp/relocation-b-uninterrupted.wav')) as full, wave.open(str(ROOT/(prefix('b','double')+'.wav'))) as resumed:
        count=resumed.getnframes();full.setpos(full.getnframes()-count)
        if full.readframes(count)!=resumed.readframes(count):raise AssertionError('relocated snapshot audio mismatch')
    print('PASS relocated uninterrupted/snapshot replay: full state and PCM suffix identical',flush=True)


def negative_checks():
    def expect(name, extra, code, message, end=10000):
        result=subprocess.run(command('a')+['--ms',str(end),'--inputs','host/scenarios/relocation-play.inputs','--out','tmp/relocation-negative-'+name]+extra,cwd=ROOT,capture_output=True,text=True)
        if result.returncode!=code or message not in result.stdout+result.stderr:
            raise AssertionError((name,result.returncode,result.stdout,result.stderr))
    reloc=(ROOT/'host/tables/relocations.csv').read_text().splitlines()
    for name,offset,end in [('rom',0x2294,10000),('ram',0x1170c,20000),('device',0x1b04,10000)]:
        path=ROOT/f'tmp/relocation-missing-{name}.csv'
        path.write_text(reloc[0]+'\n'+'\n'.join(r for r in reloc[1:] if int(r.split(',')[0],16)!=offset)+'\n')
        expect(name,['--relocation-table',str(path)],2,'unmapped relocated address',end)
    path=ROOT/'tmp/relocation-missing-low.csv'
    rows=(ROOT/'host/tables/low-vector-hooks.csv').read_text().splitlines()
    path.write_text(rows[0]+'\n'+'\n'.join(r for r in rows[1:] if not r.startswith('00616a,'))+'\n')
    expect('low',['--low-vector-hooks',str(path)],2,'unmapped relocated address')
    path=ROOT/'tmp/relocation-missing-bootstrap.csv'
    rows=(ROOT/'host/tables/control-hooks.csv').read_text().splitlines()
    path.write_text(rows[0]+'\n'+'\n'.join(r for r in rows[1:] if 'set_ram_delta' not in r)+'\n')
    expect('bootstrap',['--control-hooks',str(path)],2,'unmapped relocated address')
    path=ROOT/'tmp/relocation-missing-reset.csv';path.write_text('pc,operation\n')
    expect('reset',['--reset-hooks',str(path)],2,'RESET outside hook table')
    with (ROOT/(prefix('reference','setup')+'-trace.csv')).open() as f:first=next(csv.DictReader(f))
    path=ROOT/'tmp/relocation-missing-io.csv'
    with (ROOT/'host/tables/io-accesses.csv').open() as f:rows=list(csv.DictReader(f))
    with path.open('w') as f:
        writer=csv.DictWriter(f,['pc','address','size','direction'],lineterminator='\n');writer.writeheader()
        writer.writerows(r for r in rows if not all(int(r[k],16)==int(first[k],16) for k in ['pc','address']) or r['size']!=first['size'] or r['direction']!=first['direction'])
    expect('io',['--io-table',str(path)],2,'I/O access outside audited table')
    expect('state',['--load-state',prefix('b','setup')+'.state'],1,'state version/ROM/CPU ABI mismatch')
    expect('overlap',['--ram-base','0x100000'],1,'overlapping relocated ranges')
    expect('alignment',['--rom-base','0x100002'],1,'256-byte module alignment')
    expect('overflow',['--rom-base','0x100100000'],1,'placement exceeds 24 bits')
    bad=ROOT/'tmp/relocation-bad-rom';bad.mkdir(exist_ok=True)
    for name in ['77POK30','77POK38','77POK34','PARA200J']:
        payload=bytearray((ROOT/'rom'/name).read_bytes())
        if name=='77POK30':payload[0x300]^=1
        (bad/name).write_bytes(payload)
    expect('identity',['--rom-dir',str(bad)],1,'ROM SHA-256 mismatch')
    print('PASS negative gates: missing ROM/RAM/device/low-vector/bootstrap/RESET/I/O records; wrong state placement; overlap/alignment/overflow; corrupt ROM',flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compare-only',action='store_true')
    args=parser.parse_args()
    if not args.compare_only:
        subprocess.run(['make','roms-check'],cwd=ROOT,check=True)
        for p in PLACEMENTS:
            for name,end,load,script in MILESTONES:
                out=prefix(p,name)
                cmd=command(p)+['--ms',str(end),'--inputs',f'host/scenarios/{script}.inputs','--out',out,'--save-state',out+'.state']
                if load:cmd+=['--load-state',prefix(p,load)+'.state']
                if name=='double':cmd+=['--wav']
                subprocess.run(cmd,cwd=ROOT,check=True)
        subprocess.run(command('b')+['--ms','69500','--inputs','host/scenarios/relocation-play.inputs',
                       '--out','tmp/relocation-b-uninterrupted','--save-state','tmp/relocation-b-uninterrupted.state','--wav'],cwd=ROOT,check=True)
    replay_check()
    manifest={}
    for name,_,_,_ in MILESTONES:manifest[name]=compare(name)
    (ROOT/'tmp/relocation-ram-fields.json').write_text(json.dumps(manifest,indent=2)+'\n')
    union=bytearray(0x20000)
    for name,_,_,_ in MILESTONES:
        for i,v in enumerate(data('reference',name,'-coverage.bin')):union[i]|=v
    (ROOT/'tmp/relocation-union-coverage.bin').write_bytes(union)
    from relocation_catalog import catalog
    generated=ROOT/'tmp/relocation-catalog';catalog(generated)
    for name in ['coverage.json','covered-absolute.csv','reset-hooks.csv','rom-write-hooks.csv']:
        if (generated/name).read_bytes()!=(ROOT/'host/tables'/name).read_bytes():
            raise AssertionError('committed relocation catalog needs review: '+name)
    # Observed ROM win-state marker, checked read-only for this pinned revision.
    for name in ['win','double']:
        if data('reference',name,'-ram.bin')[0x48b00-0x79d1-0x40000]!=1:
            raise AssertionError(name+': scenario did not reach a pending win')
    negative_checks()
    print('PASS Phase 3 two-base relocation; coverage PCs',sum(v.bit_count() for v in union),'SHA256',hashlib.sha256(union).hexdigest())


if __name__=='__main__':
    main()
