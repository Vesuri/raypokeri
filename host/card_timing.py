#!/usr/bin/env python3
"""Report card admission and AY/Paula timing from a CARD_CACHE + TIME_LEDGER run.

All binary captures and ROM-derived audio sequences stay in tmp/. Corrections
subtract the measured timestamp-reader cost, not arbitrary service overhead.
"""
import argparse
from dataclasses import dataclass
from difflib import SequenceMatcher
from pathlib import Path
import re
import struct


@dataclass
class Event:
    clock: int
    cycles: int
    kind: int
    a: int
    b: int
    reads: int


def elapsed(a, b, read_cost=0):
    ticks = ((b.clock-a.clock) & 0xffffffff) - (((b.reads-a.reads) & 0xffffffff)*read_cost)
    return ticks*1000/709379


def cards(events):
    start, hit = None, False
    result = []
    for e in events:
        if e.kind == 3:
            start, hit = e, False
        elif e.kind == 4:
            hit = True
        elif e.kind == 6 and start is not None:
            result.append((start, e, hit))
            start = None
    return result


def application_delays(writes, applied, read_cost):
    """Sequence IDs establish causality even in older post-publication logs."""
    j, delays, races = 0, [], 0
    for w in writes:
        while j < len(applied) and applied[j].a < w.b:
            j += 1
        if j == len(applied):
            break
        if applied[j].clock < w.clock:
            # The old writer could be interrupted between publishing the
            # register/count and timestamping it. Do not pair that write with
            # a later envelope-only update carrying the same sequence number.
            races += 1
        else:
            delays.append(elapsed(w, applied[j], read_cost))
    return delays, races


def report(events, read_cost, reference=None, ready_cycle=0):
    batches = cards(events)
    for a, b, hit in batches:
        print(f'card cycles={a.cycles}..{b.cycles} hit={int(hit)} '
              f'wall_ms={elapsed(a,b):.3f} corrected_ms={elapsed(a,b,read_cost):.3f} '
              f'board_ms={(b.cycles-a.cycles)/8000:.3f}')
    for hit in (False, True):
        values = [elapsed(a,b,read_cost) for a,b,h in batches if h == hit]
        if values:
            print(f'cards hit={int(hit)} count={len(values)} mean_ms={sum(values)/len(values):.3f} '
                  f'min_ms={min(values):.3f} max_ms={max(values):.3f}')
    writes = [e for e in events if e.kind == 1]
    applied = [e for e in events if e.kind == 2]
    delays, races = application_delays(writes, applied, read_cost)
    if races:
        print(f'write/application publication races={races}; excluded from latency measurement')
    if delays:
        print(f'AY writes={len(writes)} applied_records={len(applied)} '
              f'max_write_to_Paula_ms={max(delays):.3f}')
    # Level changes are the envelope values actually installed by Paula VBI.
    changes = [(a,b) for a,b in zip(applied,applied[1:]) if a.b != b.b and a.a == b.a]
    if changes:
        gaps = [elapsed(a,b,read_cost) for a,b in changes]
        print(f'envelope-only advances={len(gaps)} max_wall_gap_ms={max(gaps):.3f} '
              '(observed cadence, not a host-reference deadline)')
    if reference is None:
        return
    host = []
    pattern = re.compile(r'^AY register=(\d+) value=([0-9a-f]+).* cycle=(\d+)')
    for line in reference.read_text().splitlines():
        m = pattern.match(line)
        if m:
            host.append(((int(m[1]) << 8) | int(m[2],16), int(m[3])))
    # Correlate exact register/value runs; never compare unrelated random hands.
    matcher = SequenceMatcher(None, [x[0] for x in host], [x.a for x in writes], autojunk=False)
    stretches = {False: [], True: []}
    matched = 0
    for block in matcher.get_matching_blocks():
        if block.size < 8:
            continue
        matched += block.size
        for k in range(block.size-1):
            a,b = writes[block.b+k:block.b+k+2]
            if a.cycles < ready_cycle:
                continue
            expected = (host[block.a+k+1][1]-host[block.a+k][1])/8000
            if expected < 10:
                continue  # intra-update bus traffic is not a sound duration
            overlap = [h for ca,cb,h in batches if ca.clock < b.clock and cb.clock > a.clock]
            if not overlap:
                continue
            stretch = elapsed(a,b,read_cost)-expected
            stretches[all(overlap)].append(stretch)
            print(f'audio gap cycles={a.cycles}..{b.cycles} cached_cards={int(all(overlap))} '
                  f'host_ms={expected:.3f} wall_ms={elapsed(a,b,read_cost):.3f} '
                  f'stretch_ms={stretch:.3f}')
    print(f'AY exact-run matched writes={matched}/{len(writes)}')
    for hit,values in stretches.items():
        if values:
            print(f'audio card_overlap_hit={int(hit)} samples={len(values)} '
                  f'max_stretch_ms={max(values):.3f}')


def self_test():
    a=Event(100,0,3,0,0,1);b=Event(719479,8000000,6,0,0,101)
    assert abs(elapsed(a,b,100)-1000)<1e-9
    e=[a,Event(200,1,4,1,1,2),b]
    assert cards(e)==[(a,b,True)]
    assert cards([a,a,b])==[(a,b,False)]
    assert cards([Event(0,0,6,0,0,0)])==[]
    writes=[Event(1010,0,1,0x700,510,0),Event(2010,0,1,0x701,511,0)]
    applied=[Event(1000,0,2,510,0,0),Event(2000,0,2,510,1,0),Event(2100,0,2,511,1,0)]
    delays,races=application_delays(writes,applied,0)
    assert races==1 and len(delays)==1 and abs(delays[0]-90*1000/709379)<1e-12
    assert application_delays(writes,[],0)==([],0)
    print('PASS: timestamp correction, sequence pairing and VBI publication races')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--events',type=Path)
    parser.add_argument('--log',type=Path)
    parser.add_argument('--reference',type=Path)
    parser.add_argument('--self-test',action='store_true')
    args=parser.parse_args()
    if args.self_test:
        self_test();return
    if not args.events or not args.log:
        parser.error('--events and --log are required')
    log=args.log.read_text()
    dropped=re.search(r'EVENTS count=\d+ dropped=(\d+)',log)
    if not dropped or int(dropped[1]):
        raise SystemExit('incomplete or overflowing event capture')
    cost=re.search(r'LEDGER readcost=(\d+)',log)
    if not cost:
        raise SystemExit('missing timestamp-reader calibration')
    events=[Event(*r) for r in struct.iter_unpack('>6I',args.events.read_bytes())]
    ready=re.search(r"READY cycles=(\d+)",log)
    report(events,int(cost[1])/256,args.reference,int(ready[1]) if ready else 0)


if __name__=='__main__':
    main()
