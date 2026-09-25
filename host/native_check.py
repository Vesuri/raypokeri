#!/usr/bin/env python3
"""Compare all native RAM against Musashi at the actual Amiga allocation bases.

The native capture must be a completed diagnostic replay. No timer, RNG, stack,
or pointer bytes are masked: the host reruns at the same addresses instead.
"""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', type=Path, default=ROOT/'amiga/.run/gdb-out.log')
    parser.add_argument('--ram', type=Path, default=ROOT/'tmp/native-amiga-ram.bin')
    parser.add_argument('--out', default='tmp/native-comparison', help='reference capture prefix')
    parser.add_argument('--inputs', default='host/scenarios/relocation-play.inputs')
    parser.add_argument('--live-boot', action='store_true', help='compare the captured replay-to-live boundary')
    parser.add_argument('--watchdog-stop', action='store_true',
                        help='with --live-boot, accept an intentional first-watchdog stop; verifies boot only')
    args = parser.parse_args()
    if args.watchdog_stop and not args.live_boot:
        parser.error('--watchdog-stop requires --live-boot')
    log = args.log.read_text()
    if "Error in sourced command file" in log or "Remote connection closed" in log:
        raise SystemExit("debugger capture failed; do not use a stale RAM file")
    match = re.search(r'native status=(\d+) count=(\d+) cycles=(\d+) irqs=(\d+) pc=([0-9a-f]+)', log)
    bases = re.search(r'bases rom=([0-9a-f]+) ram=([0-9a-f]+) guard=([0-9a-f]+)', log)
    if args.live_boot:
        boot = re.search(r'boot count=(\d+) cycles=(\d+) irqs=(\d+) pc=([0-9a-f]+)', log)
        final = re.search(r'native status=(\d+) boot=1 ', log)
        clean = final and int(final[1]) in (3,4) and re.search(r'(?:native error=|\$1 = )0x0\b', log)
        stopped = (args.watchdog_stop and final and int(final[1]) == 0xdead
                   and re.search(r'live watchdog resets=1 first PC=[0-9a-f]+ first elapsed cycles=\d+', log)
                   and re.search(r'\$\d+ = .*\"live watchdog expired\"', log))
        if not boot or not (clean or stopped):
            raise SystemExit('hybrid run neither exited cleanly nor reached the requested first-watchdog stop')
        if stopped:
            print('BOOT ONLY: live watchdog expired; this comparison cannot validate live execution', flush=True)
    if (not match and not args.live_boot) or not bases:
        raise SystemExit('missing native diagnostic counters or allocation bases')
    if "vectors restored=1" not in log:
        raise SystemExit("native vector restoration was not verified")
    if args.live_boot:
        status=2
        count,cycles,irqs=map(int,boot.groups()[:3]);pc=int(boot[4],16)
    else:
        status, count, cycles, irqs = map(int, match.groups()[:4])
        pc = int(match[5], 16)
    if status != 2:
        raise SystemExit('native replay has not completed successfully')
    native = args.ram.read_bytes()
    if len(native) != 0x40000:
        raise SystemExit('native RAM capture must contain all 262144 bytes')
    command = [str(ROOT/'build/pokeri-host'), '--devices', '--serial-peer',
               '--system-hz', '100', '--input-hz', '50', '--watchdog-ms', '400',
               '--watchdog-reset-us', '50000', '--ay-clock', '1000000',
               '--palette-rom', '0', '--stall-instructions', '100000000',
               '--bypass-module-checksums', '--io-table', 'host/tables/io-accesses.csv',
               '--inputs', args.inputs, '--instructions', str(count),
               '--out', args.out]
    for option, value in zip(['--rom-base', '--ram-base', '--device-base'], bases.groups()):
        command.extend([option, '0x'+value])
    subprocess.run(command, cwd=ROOT, check=True)
    context = (ROOT/(args.out+'-context.txt')).read_text()
    device = (ROOT/(args.out+'-devices.txt')).read_text()
    endpoint = re.search(r'instructions=(\d+) cycles=(\d+) pc=([0-9a-f]+)', context)
    host_irqs = re.search(r'IRQs=(\d+)', device)
    rom_base,ram_base,_ = (int(value,16) for value in bases.groups())
    expected_pc=rom_base+pc if pc<0x40000 else ram_base+pc-0x40000
    if not endpoint or not host_irqs or (int(endpoint[1]), int(endpoint[2]), int(endpoint[3],16), int(host_irqs[1])) != (count,cycles,expected_pc,irqs):
        raise SystemExit('host/native instruction, cycle, PC or IRQ boundary differs')
    reference = (ROOT/(args.out+'-ram.bin')).read_bytes()
    differences = [i for i,(a,b) in enumerate(zip(reference,native)) if a != b]
    if len(reference) != len(native) or differences:
        for i in differences[:32]:
            print(f'{0x40000+i:05x}: host={reference[i]:02x} native={native[i]:02x}')
        raise SystemExit(f'FAIL: {len(differences)} differing RAM bytes')
    print(f'PASS: all {len(native)} RAM bytes match at {count} instructions, {cycles} cycles, {irqs} IRQs')

if __name__ == '__main__':
    main()
