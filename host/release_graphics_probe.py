#!/usr/bin/env python3
"""Bounded read-only Double command probes, retaining native cache fast paths.

Only C++ CardBackCache::command -> Hd63484::finishCommand intervals are seen.
Assembly-borrowed completions, command feeding and publication are not included.
"""
import argparse
from collections import defaultdict
from pathlib import Path
import re
from release_timing import elapsed

PROBES = r'''set $graphics_end = 0
set $graphics_serial = 0
break *(&_ZN6pokeri13CardBackCache7commandERNS_7Hd63484EPKtj)
set $graphics_begin_bp = $bpnum
disable $graphics_begin_bp
commands
silent
if nativeCycles >= $graphics_end
 disable $graphics_begin_bp
 disable $graphics_finish_bp
else
 set $graphics_serial = $graphics_serial + 1
 set $graphics_words = *(unsigned short**)($sp+12)
 set $graphics_count = *(unsigned long*)($sp+16)
 printf "GRAPHICS begin id=%u cycle=%u frame=%u beam=%u count=%u words=",$graphics_serial,nativeCycles,pendingFrames,(*(unsigned long*)0xdff004>>8)&511,$graphics_count
 set $gi = 0
 while $gi < $graphics_count && $gi < 64
  printf "%04x,",$graphics_words[$gi]
  set $gi = $gi + 1
 end
 printf "\n"
end
continue
end
break *(&_ZN6pokeri7Hd6348413finishCommandEjb)
set $graphics_finish_bp = $bpnum
disable $graphics_finish_bp
commands
silent
printf "GRAPHICS finish id=%u cycle=%u frame=%u beam=%u group=%u done=%u\n",$graphics_serial,nativeCycles,pendingFrames,(*(unsigned long*)0xdff004>>8)&511,*(unsigned long*)($sp+8),*(unsigned long*)($sp+12)
continue
end
'''
ARM = r'''if *(unsigned long*)($sp+4) == 0x55 && *(unsigned long*)($sp+8) != 0
 set $graphics_end = nativeCycles + 8000000
 enable $graphics_begin_bp
 enable $graphics_finish_bp
 printf "GRAPHICS armed cycle=%u\n",nativeCycles
end
'''


def prepare(template):
    for marker in ('break *(&amigaInputKey)', 'printf "RELEASE key cycle='):
        if template.count(marker) != 1:
            raise ValueError('expected one Double key probe: ' + marker)
    return template.replace('break *(&amigaInputKey)', PROBES + 'break *(&amigaInputKey)', 1).replace(
        'printf "RELEASE key cycle=', ARM + 'printf "RELEASE key cycle=', 1)


def commands(text):
    pending = {}
    result = []
    for line in text.splitlines():
        if not line.startswith(('GRAPHICS begin ', 'GRAPHICS finish ')):
            continue
        fields = {k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', line.split(' words=')[0])}
        serial = fields['id']
        if line.startswith('GRAPHICS begin '):
            if serial in pending:
                raise ValueError('duplicate graphics start')
            words = [int(w, 16) for w in line.split('words=')[1].split(',') if w]
            if len(words) != fields['count']:
                raise ValueError('truncated graphics command')
            pending[serial] = (fields, words)
        else:
            if serial not in pending:
                raise ValueError('graphics completion without start')
            start, words = pending.pop(serial)
            if words[0] >> 10 != fields['group'] or fields['done'] != 1:
                raise ValueError('graphics opcode/completion mismatch')
            duration = elapsed(fields) - elapsed(start)
            if duration < 0:
                raise ValueError('graphics timestamps moved backwards')
            result.append(dict(start=start, end=fields, words=words, seconds=duration))
    if pending:
        raise ValueError('incomplete graphics commands')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    rows = commands(args.log.read_text())
    if not rows:
        raise ValueError('no completed graphics probes')
    groups = defaultdict(list)
    for row in rows:
        groups[row['words'][0] >> 10].append(row['seconds'] * 1000)
    print('Observed C++ intervals only; borrowed assembly completions excluded.')
    print('group  count  total_ms  max_ms')
    for group, times in sorted(groups.items(), key=lambda item: sum(item[1]), reverse=True):
        print(f'{group:5d} {len(times):6d} {sum(times):9.3f} {max(times):7.3f}')
    print(f'Total: {len(rows)} commands, {sum(r["seconds"] for r in rows)*1000:.3f} ms')


if __name__ == '__main__':
    main()
