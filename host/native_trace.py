#!/usr/bin/env python3
"""Attribute FS-UAE instruction traces of the Amiga TRACE_CODE build.

Captures come from `monitor profile` (see amiga/trace.gdb). The emulator records
every executed instruction with its cycle cost; the traced executable contains
no timers or counters. TRACE_CODE places the Board storage inside the first code
hunk, so original 68008 instructions keep their PCs.

  python3 host/native_trace.py --run amiga/.run/trace-x [--prefix startup]
      [--fields FIRST LAST] [--top 40] [--timeline]

The run directory holds Pokeri.elf, gdb-out.log (TRACE base/vector lines) and
trace-<prefix>-NNN.bin files. Cycles are emulated 68020 cycles (14.19 MHz on
the A1200 preset, 284,204 per PAL field); DMA contention is folded into the
instruction that waited. host/trace_reduce.cpp performs the per-record pass.
"""
import argparse
import bisect
import collections
import csv
import os
from pathlib import Path
import re
import struct
import subprocess
import shutil
import sys

ROOT = Path(__file__).resolve().parent.parent
BIN = Path.home() / '.local/opt/bin'
FIELD = 284204.0
KINDS = ['lineA', 'trace', 'trap', 'fault', 'vbi', 'cia_a', 'audio', 'cia_b', 'exec_l1', 'exec_l5', 'exec_l7']
ENTRY_LABELS = {'nativeLineA': 'lineA', 'nativeTrace': 'trace', 'nativeFault': 'fault',
                'nativeLevel3': 'vbi', 'nativeLevel2': 'cia_a', 'nativeLevel4': 'audio', 'nativeLevel6': 'cia_b'}
GUEST_EVENTS = {0x0C06: 'tick', 0x0DF4: 'input_irq', 0x2E26: 'fifo_irq', 0xE058: 'sound_seq',
                0x0D58: 'sound_reg'}
NATIVE_EVENTS = {'nativePrepareInner': 'prepare', 'nativeInstallVectors': 'prepared',
                 'PaulaAy::write': 'ay', 'AmigaScreen::present': 'present', 'nativeDispatch': 'dispatch',
                 'nativeLevel3': 'vbi', 'nativeLineA': 'linea', 'nativeTrace': 'trace_entry',
                 'pushException': 'virq', 'nativeShortPromote': 'promote'}
# Subsystems, first match wins, applied to the demangled leaf function.
GROUPS = [
    ('entry/exit asm (short paths, traps, trace)', r'^native(LineA|Short|Trap|Trace|GameTrace|Resume|Fast|Slow|Fault|Exit|Handler|FifoControl|Video|Feed|Ring|Level)'),
    ('Paula/AY audio', r'PaulaAy|AyEnvelope|Ay38912|AyAudio'),
    ('presentation (AmigaScreen)', r'AmigaScreen|FrameSwap|OutputPanel'),
    ('card cache / cached raster', r'CardBackCache|CachedRaster|nativeRaster|CardDamage|cardCache'),
    ('Amiga blitter/planar rendering', r'AmigaSurface|AmigaHardware|PlanarSurface|Bitmap|blitter'),
    ('HD63484 command model/drawing', r'Hd63484|pokeri::Hd|Drawing'),
    ('board devices (PIA/ACIA/serial/tick)', r'Board::|Pia6821|Acia6850|SerialPeer|NvRam|Nvram'),
    ('prepared/generic hook execution', r'executePreparedHook|executeHook|PreparedBus|HookBus|Bus::|PreparedHook|executeLineA'),
    ('startup/cabinet setup', r'Startup|coldSetup|CabinetInput|RetainedAccounting'),
    ('native scheduler/clock (C)', r'nativeDispatch|nativeClock|accountGuest|LiveClock|pushException|shuffle|Shuffle|checkGuard|guardRange|idleBudget|nativeDelay|liveInputs|amigaInput|AmigaInput|setSr|nativeBatch|nativeShortPia|nativeShortIo|nativeShortVideo|nativeFifo|revokeRaster|diagnosticKeys'),
    ('runtime helpers', r'^(mem(set|cpy|move)|__|operator|pokeri(Allocate|Free|Memset))'),
]


def tool(name):
    found = shutil.which(name)
    return found or str(BIN / name)


def demangle(names):
    out = subprocess.run([shutil.which('c++filt') or 'c++filt'], input='\n'.join(names) + '\n',
                         capture_output=True, text=True, check=True).stdout.split('\n')
    return out[:len(names)]


def short(name):
    name = re.sub(r'\.(lto_priv|constprop|isra|part|cold)\.\d+', '', name)
    depth, cut = 0, None
    for i, ch in enumerate(name):
        if ch == '(' and depth == 0 and cut is None and not name.startswith('operator'):
            cut = i
        depth += ch == '<'
        depth -= ch == '>'
    return (name[:cut] if cut else name).replace('pokeri::', '')


def load_elf(elf):
    table = subprocess.run([tool('m68k-amiga-elf-objdump'), '-t', elf], capture_output=True, text=True, check=True).stdout
    raw = []
    for line in table.split('\n'):
        m = re.match(r'^([0-9a-f]{8}) (.{7}) \.text\s+([0-9a-f]+) (.+)$', line)
        if m and not m.group(4).startswith('.L'):
            raw.append((int(m.group(1), 16), int(m.group(3), 16), m.group(4)))
    names = demangle([n for _, _, n in raw])
    symbols = sorted((a, s, short(n)) for (a, s, _), n in zip(raw, names))
    dis = subprocess.run([tool('m68k-amiga-elf-objdump'), '-d', '--no-show-raw-insn', elf],
                         capture_output=True, text=True, check=True).stdout
    calls, rtes, pending = {}, [], None
    for line in dis.split('\n'):
        m = re.match(r'^\s*([0-9a-f]+):\s+(\S+)', line)
        if not m:
            continue
        address = int(m.group(1), 16)
        if pending is not None:
            calls[pending] = address
            pending = None
        mnemonic = m.group(2)
        if mnemonic == 'jsr' or mnemonic.startswith('bsr'):
            pending = address
        elif mnemonic == 'rte':
            rtes.append(address)
    text = subprocess.run([tool('m68k-amiga-elf-objdump'), '-h', elf], capture_output=True, text=True, check=True).stdout
    size = int(re.search(r'\.text\s+([0-9a-f]+)', text).group(1), 16)
    return symbols, calls, rtes, size


def text_word(elf, address):
    out = subprocess.run([tool('m68k-amiga-elf-objdump'), '-s', '-j', '.text', f'--start-address={address:#x}',
                          f'--stop-address={address + 2:#x}', elf], capture_output=True, text=True, check=True).stdout
    m = re.search(r'^ [0-9a-f]+ ([0-9a-f]{4})', out, re.M)
    return int(m.group(1), 16) if m else None


def trace_arm_sites(disassembly, labels):
    """Identify executed ORI.W #$8000,(SP) within each interrupt wrapper."""
    instructions = []
    for line in disassembly.splitlines():
        match = re.match(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{4}\s+)+)(\S.*)$', line)
        if match:
            instructions.append((int(match[1], 16), bytes.fromhex(match[2])))
    result = {}
    for level in (2, 3, 4, 6):
        start, end = labels[f'nativeLevel{level}'], labels[f'nativeChainLevel{level}']
        sites = [pc for pc, raw in instructions if start <= pc < end and raw == bytes.fromhex('00578000')]
        if len(sites) != 1:
            raise ValueError(f'level {level}: trace-arm instruction is not unique')
        result[sites[0]] = f'irq_level_{level}'
    return result


def card_counter_sites(disassembly, offsets):
    """Find unique field increments after proving the ABI `this` register.

    Fail closed when compiler allocation/prologue changes; never identify a
    counter from ADDQ alone or from a fixed offset into the function.
    """
    instructions = []
    for line in disassembly.splitlines():
        m = re.match(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{4}\s+)+)(\S.*)$', line)
        if m:
            instructions.append((int(m[1], 16), bytes.fromhex(m[2]), m[3]))
    if len(instructions) < 3:
        raise ValueError('missing card command prologue')
    index, stack = 0, 4  # return address, then first C++ argument
    opcode = int.from_bytes(instructions[0][1][:2], 'big')
    framed = opcode == 0x4e55  # LINK.W A5,#locals
    if framed:
        index += 1
    elif opcode & 0xf1ff == 0x518f:  # SUBQ.L #n,SP
        stack += (opcode >> 9) & 7 or 8
        index += 1
    saved = instructions[index][1]
    if len(saved) != 4 or saved[:2] != bytes.fromhex('48e7'):
        raise ValueError('unrecognized card command register save')
    stack += int.from_bytes(saved[2:], 'big').bit_count() * 4
    index += 1
    loaded = instructions[index][1]
    register = (int.from_bytes(loaded[:2], 'big') >> 9) & 7
    expected_opcode = (0x206d if framed else 0x206f) | register << 9
    expected_this = expected_opcode.to_bytes(2, 'big') + (8 if framed else stack).to_bytes(2, 'big')
    if loaded != expected_this or register not in (2, 3, 4, 6):
        raise ValueError('card counter base is not a preserved first-argument register')
    # Callees preserve this register. Refuse explicit reassignment.

    for _, _, text in instructions[index+1:]:
        if re.search(fr',a{register}$', text) and not text.startswith('cmp'):
            raise ValueError('card command reassigns its counter base')
    sites = {}
    for field, offset in offsets.items():
        expected = (0x52a8 | register).to_bytes(2, 'big') + offset.to_bytes(2, 'big')
        matches = [pc for pc, raw, _ in instructions if raw == expected]
        if len(matches) != 1:
            raise ValueError(f'card {field} has {len(matches)} field increments')
        sites[field] = matches[0]
    return sites


def card_counters(elf, address, size, fields=('starts', 'hits')):
    args = [tool('m68k-amiga-elf-gdb'), '-nx', '-batch', str(elf)]
    for field in fields:
        args += ['-ex', f'p/x (unsigned long)&((pokeri::CardBackCache*)0)->{field}']
    debug = subprocess.run(args, capture_output=True, text=True, check=True).stdout
    values = re.findall(r'^\$\d+ = 0x([0-9a-f]+)$', debug, re.M)
    if len(values) != len(fields):
        raise ValueError('missing card counter DWARF offsets')
    dis = subprocess.run([tool('m68k-amiga-elf-objdump'), '-d',
                          f'--start-address={address:#x}', f'--stop-address={address+size:#x}',
                          str(elf)], capture_output=True, text=True, check=True).stdout
    return card_counter_sites(dis, dict(zip(fields, (int(v, 16) for v in values))))


def header_sections(path):
    with open(path, 'rb') as f:
        head = f.read(8)
        fields, count = struct.unpack('<2I', head)
        return fields, list(struct.unpack(f'<{count}I', f.read(4 * count)))


def rom_names():
    rows = []
    with open(ROOT / 'disasm/symbols.csv') as f:
        for row in csv.DictReader(f):
            if row['type'] in ('function', 'label', 'code'):
                rows.append((int(row['addr'], 16), row['name']))
    rows.sort()
    return rows


def configure(run, elf, traces):
    log = (run / 'gdb-out.log').read_text(errors='replace')
    base = re.search(r'TRACE base rom=([0-9a-f]+)', log)
    if not base:
        sys.exit('gdb-out.log lacks the TRACE base line')
    rom_base = int(base.group(1), 16)
    vectors = re.search(r'TRACE vectors l1=([0-9a-f]+) l5=([0-9a-f]+) l7=([0-9a-f]+)', log)
    _, sections = header_sections(traces[0])
    symbols, calls, rtes, size = load_elf(elf)
    by_name = {}
    for address, _, name in symbols:
        by_name.setdefault(name, address)
    lines = [f'kind {k}' for k in KINDS]
    lines.append(f'guest {rom_base - sections[0]:x}')
    lines.append(f'text {size:x}')
    lines.append(f'full {by_name["nativeDispatch"]:x}')
    for label, kind in ENTRY_LABELS.items():
        lines.append(f'entry {by_name[label]:x} {KINDS.index(kind)}')
    disassembly = subprocess.check_output([tool('m68k-amiga-elf-objdump'), '-d', str(elf)], text=True)
    for pc, origin in trace_arm_sites(disassembly, by_name).items():
        lines.append(f'tracearm {pc:x} {origin}')
    lines.append(f'traceresume {by_name["nativeResume"]:x}')
    for n in range(16):
        lines.append(f'entry {by_name[f"nativeTrap{n}"]:x} {KINDS.index("trap")}')
    if vectors:
        for value, kind in zip(vectors.groups(), ('exec_l1', 'exec_l5', 'exec_l7')):
            lines.append(f'kickentry {int(value, 16):x} {KINDS.index(kind)}')
    lines += [f'call {a:x} {n:x}' for a, n in calls.items()]
    lines += [f'rte {a:x}' for a in rtes]
    for name, event in NATIVE_EVENTS.items():
        if name in by_name:
            lines.append(f'event {by_name[name]:x} {event}')
    card = by_name.get('CardBackCache::command')
    if card is not None:
        try:
            size = next(size for address, size, name in symbols if address == card and name == 'CardBackCache::command')
            sites = card_counters(elf, card, size)
            lines += [f'event {sites["starts"]:x} card_begin', f'event {sites["hits"]:x} card_hit']
        except (ValueError, subprocess.SubprocessError, OSError) as error:
            print(f'note: card counters omitted: {error}')
    admit = next(((address, length) for address, length, name in symbols if name == 'CardBackCache::admit'), None)
    if admit:
        try:
            sites = card_counters(elf, *admit, fields=('guardMisses',))
            lines.append(f'event {sites["guardMisses"]:x} card_guard_miss')
        except (ValueError, subprocess.SubprocessError, OSError) as error:
            print(f'note: card guard counter omitted: {error}')
    for pc, event in GUEST_EVENTS.items():
        lines.append(f'gevent {pc:x} {event}')
    return '\n'.join(lines) + '\n', symbols, rom_base, sections[0]


def reducer():
    source = ROOT / 'host/trace_reduce.cpp'
    binary = ROOT / 'tmp/trace_reduce'
    if not binary.exists() or binary.stat().st_mtime < source.stat().st_mtime:
        binary.parent.mkdir(exist_ok=True)
        subprocess.run(['c++', '-O2', '-std=c++17', '-o', str(binary), str(source)], check=True)
    return binary


def read_tsv(path):
    with open(path) as f:
        return list(csv.DictReader(f, delimiter='\t'))


class Symbols:
    def __init__(self, symbols):
        self.starts = [a for a, _, _ in symbols]
        self.names = [n for _, _, n in symbols]

    def __call__(self, pc):
        i = bisect.bisect_right(self.starts, pc) - 1
        return self.names[i] if i >= 0 else '?'


def group(name):
    for label, pattern in GROUPS:
        if re.search(pattern, name):
            return label
    return 'other native'


def ms(cycles):
    return 1e3 * cycles / (FIELD * 50)


def report(out, symbols, top, timeline, fields_window, tree=0, site=None):
    name = Symbols(symbols)
    summary = {r[0]: int(r[1]) for r in csv.reader(open(out / 'summary.tsv'), delimiter='\t')}
    rows = read_tsv(out / 'fields.tsv')
    total = sum(int(r['cycles']) for r in rows)
    if not total:
        sys.exit('no fields in the selected window')
    cats = collections.Counter()
    for r in rows:
        for k, v in r.items():
            if k not in ('field', 'cycles', 'idle') and not k.startswith('ev_'):
                cats[k] += int(v)
    events = collections.Counter()
    for r in rows:
        for k, v in r.items():
            if k.startswith('ev_'):
                events[k[3:]] += int(v)
    prepared = next((i for i, row in enumerate(rows) if int(row.get('ev_prepared', 0))), None)
    if prepared is not None:
        low = sum(int(row['cycles']) for row in rows[:prepared])
        high = low + int(rows[prepared]['cycles'])
        print(f'Preparation reaches vector installation in field {rows[prepared]["field"]}; '
              f'{ms(low):.1f}–{ms(high):.1f} ms from the selected capture start. '
              'The remainder of this report may include original execution.')
    wall = total / (FIELD * 50)
    ticks = events['tick']
    print(f'== {len(rows)} PAL fields, {wall:.3f} s emulated; idle (STOP) {summary.get("idle", 0) / total:.1%}')
    print(f'   board ticks {ticks} (= {ticks / 100:.2f} board-s, board/wall {ticks / 100 / wall:.3f}); '
          f'AY writes {events["ay"]}; card begin/hit {events.get("card_begin", 0)}/{events.get("card_hit", 0)}; '
          f'presents {events["present"]}; VBI {events["vbi"]}')
    if 'card_guard_miss' in events:
        print(f'   refused card-cache admissions: {events["card_guard_miss"]}')
    print(f'   entries: Line-A {events["linea"]}, trace {events["trace_entry"]}, full dispatch {events["dispatch"]}, '
          f'virtual IRQs {events["virq"]}, promotions {events["promote"]}')
    origins = out / 'trace-origins.tsv'
    if origins.exists():
        origin_rows = read_tsv(origins)
        if sum(int(row['entries']) for row in origin_rows) != events['trace_entry']:
            raise ValueError('trace-origin totals do not match observed trace entries')
        print('   trace origins (direct preceding arm + one guest instruction; otherwise unresolved):')
        for row in origin_rows:
            print(f'     {row["origin"]}: {row["entries"]}')
    print('\n-- where the time goes (share of all emulated cycles)')
    for k, v in cats.most_common():
        if v:
            label = k.replace('top_', 'service entered from original code: ').replace('nested_', 'Amiga IRQ nested in a service: ')
            print(f'   {label:62s} {100 * v / total:6.2f}%  {ms(v):9.1f} ms')
    # Native self time by subsystem and function.
    kind_pc = read_tsv(out / 'kind_pc.tsv')
    by_group = collections.Counter()
    by_func = collections.Counter()
    by_func_nested = collections.Counter()
    for r in kind_pc:
        pc = int(r['pc'], 16)
        cyc = int(r['cycles'])
        if pc == 0xffffffff:
            label = '[unrecorded code]'
        elif pc >= 0xf80000:
            label = '[Kickstart]'
        else:
            label = name(pc)
        grp = label if label.startswith('[') else group(label)
        by_group[(r['nested'] == '1', grp)] += cyc
        (by_func_nested if r['nested'] == '1' else by_func)[label] += cyc
    print('\n-- native/OS self time by subsystem (top-level services | nested Amiga IRQs)')
    groups = collections.Counter()
    for (nested, g), v in by_group.items():
        groups[g] += v
    for g, v in groups.most_common():
        print(f'   {g:52s} {100 * v / total:6.2f}%  (nested {100 * by_group[(True, g)] / total:5.2f}%)')
    native = read_tsv(out / 'native.tsv')
    funcs = collections.Counter()
    incl = {}
    calls = {}
    for r in native:
        pc = int(r['pc'], 16)
        funcs[name(pc)] += int(r['self'])
        if int(r['incl']) or int(r['calls']):
            n = name(pc)
            incl[n] = incl.get(n, 0) + int(r['incl'])
            calls[n] = calls.get(n, 0) + int(r['calls'])
    print(f'\n-- top {top} native functions by self time (all contexts)')
    for f, v in funcs.most_common(top):
        print(f'   {f[:70]:70s} {100 * v / total:6.2f}%')
    print(f'\n-- top {top} called native functions by inclusive time (calls, us per call)')
    for f, v in sorted(incl.items(), key=lambda x: -x[1])[:top]:
        c = calls.get(f, 0)
        print(f'   {f[:62]:62s} {100 * v / total:6.2f}%  {c:8d} {1e6 * v / max(1, c) / (FIELD * 50):8.1f}')
    if site is not None:
        selected = summary['selected_cycles']
        print(f'\n-- Line-A at ${site:05X}: {ms(selected):.1f} ms own service time; nested IRQs excluded')
        functions = collections.defaultdict(lambda: [0, 0, 0])
        for row in read_tsv(out / 'site-native.tsv'):
            entry = functions[name(int(row['pc'], 16))]
            for i, key in enumerate(('self', 'incl', 'calls')):
                entry[i] += int(row[key])
        print('   function                                                       self ms   incl ms     calls  us/call')
        for func, (own, inclusive, count) in sorted(functions.items(), key=lambda item: -max(item[1][:2]))[:top]:
            print(f'   {func[:60]:60s} {ms(own):9.2f} {ms(inclusive):9.2f} {count:9d} {ms(inclusive)*1000/max(1,count):8.1f}')
    # Guest code.
    rom = rom_names()
    starts = [a for a, _ in rom]
    guest = collections.Counter()
    for r in read_tsv(out / 'guest.tsv'):
        pc = int(r['pc'], 16)
        i = bisect.bisect_right(starts, pc) - 1
        label = f'{rom[i][1]} (${rom[i][0]:X})' if i >= 0 and pc < 0x40000 else ('RAM code' if pc >= 0x40000 else f'${pc:X}')
        guest[label] += int(r['cycles'])
    print('\n-- original code by nearest named ROM routine (self)')
    for g, v in guest.most_common(15):
        print(f'   {g[:60]:60s} {100 * v / total:6.2f}%')
    # Service episodes by entry site.
    sites = read_tsv(out / 'sites.tsv')
    agg = collections.defaultdict(lambda: [0, 0, 0, 0])
    for r in sites:
        pc = int(r['guest_pc'], 16)
        a = agg[(r['kind'], pc)]
        a[0] += int(r['count']); a[1] += int(r['cycles']); a[2] += int(r['full']); a[3] += int(r['full_cycles'])
    print(f'\n-- top {top} service entry sites (own cycles, nested IRQs excluded)')
    print('   kind    guest-PC   routine                              count  share  us/each  full  us/full')
    for (kind, pc), (n, cyc, full, fcyc) in sorted(agg.items(), key=lambda x: -x[1][1])[:top]:
        i = bisect.bisect_right(starts, pc) - 1
        label = rom[i][1] if i >= 0 and pc < 0x40000 else ('RAM' if pc < 0x80000 else '?')
        us = 1e6 * cyc / max(1, n) / (FIELD * 50)
        ufull = 1e6 * fcyc / max(1, full) / (FIELD * 50) if full else 0
        print(f'   {kind:7s} ${pc:05X}    {label[:34]:34s} {n:7d} {100 * cyc / total:5.2f}% {us:8.1f} {full:6d} {ufull:8.1f}')
    if tree:
        edges = collections.defaultdict(dict)
        for r in read_tsv(out / 'edges.tsv'):
            edges[int(r['parent'], 16)][int(r['child'], 16)] = int(r['cycles'])
        def label(pc):
            return '[self]' if pc == 0xfffffffe else name(pc)
        def walk(node, depth, seen):
            kids = sorted(edges.get(node, {}).items(), key=lambda x: -x[1])
            for child, cyc in kids:
                if 100 * cyc / total < tree or depth > 14:
                    continue
                print(f'   {"  " * depth}{100 * cyc / total:6.2f}%  {label(child)[:80]}')
                if child != 0xfffffffe and child not in seen:
                    walk_child(node, child, depth + 1, seen | {child})
        def walk_child(parent, node, depth, seen):
            # Edges are keyed by immediate parent only; a callee reached from
            # several parents shows its merged subtree under each of them.
            walk(node, depth, seen)
        print(f'\n-- call tree by service kind (inclusive; merged per callee; >= {tree}% shown)')
        for root in sorted((r for r in edges if r < 0x100), key=lambda r: -sum(edges[r].values())):
            nested = root & 0x80
            kind = KINDS[(root & 0x7f) - 1] if (root & 0x7f) else 'root'
            share = 100 * sum(edges[root].values()) / total
            if share >= tree:
                print(f'   {share:6.2f}%  [{"nested " if nested else ""}{kind}]')
                walk(root, 1, set())
    if timeline:
        print('\n-- per field (ms of the 20 ms field; ticks = 10 ms board ticks executed)')
        keys = [k for k in rows[0] if k.startswith(('top_', 'nested_'))]
        head = 'field  guest  ' + ' '.join(f'{k.replace("top_", "").replace("nested_", "n.")[:7]:>7s}' for k in keys)
        print('   ' + head + '  ticks  ay  card  pres')
        for r in rows:
            cyc = int(r['cycles'])
            g = int(r['guest_delay']) + int(r['guest_other'])
            vals = ' '.join(f'{ms(int(r[k])):7.2f}' for k in keys)
            print(f'   {int(r["field"]):5d} {ms(g):6.2f}  {vals}  {r["ev_tick"]:>5s} {r["ev_ay"]:>3s} '
                  f'{r.get("ev_card_hit", "0"):>5s} {r["ev_present"]:>4s}')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--run', type=Path, required=True)
    ap.add_argument('--prefix', default='')
    ap.add_argument('--fields', nargs=2, type=int)
    ap.add_argument('--top', type=int, default=30)
    ap.add_argument('--timeline', action='store_true')
    ap.add_argument('--tree', type=float, default=0, help='print call tree nodes above this share (percent)')
    ap.add_argument('--site', type=lambda value: int(value, 0), help='attribute outer Line-A service at this original PC, excluding nested IRQs')
    ap.add_argument('--out', type=Path)
    args = ap.parse_args()
    pattern = re.compile(rf'trace-{re.escape(args.prefix)}.*?(\d+)\.bin$' if args.prefix else r'trace-.*?(\d+)\.bin$')
    traces = sorted((p for p in args.run.iterdir() if pattern.search(p.name)), key=lambda p: int(pattern.search(p.name).group(1)))
    if not traces:
        sys.exit('no trace captures found')
    elf = args.run / 'Pokeri.elf'
    config, symbols, rom_base, hunk = configure(args.run, elf, traces)
    out = args.out or args.run / (f'reduced-{args.prefix or "all"}' + (f'-{args.fields[0]}-{args.fields[1]}' if args.fields else ''))
    out.mkdir(exist_ok=True)
    (out / 'config.txt').write_text(config)
    cmd = [str(reducer()), str(out / 'config.txt'), str(out)] + [str(t) for t in traces]
    if args.fields:
        cmd += ['--fields', str(args.fields[0]), str(args.fields[1])]
    if args.site is not None:
        cmd += ['--site', str(args.site)]
    subprocess.run(cmd, check=True)
    print(f'{len(traces)} capture(s), hunk0 {hunk:#x}, romBase {rom_base:#x}')
    report(out, symbols, args.top, args.timeline, args.fields, args.tree, args.site)


if __name__ == '__main__':
    main()
