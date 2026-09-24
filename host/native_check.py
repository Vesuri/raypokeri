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
    parser.add_argument('--inputs', default='host/scenarios/relocation-play.inputs')
    args = parser.parse_args()
    log = args.log.read_text()
    if "Error in sourced command file" in log or "Remote connection closed" in log:
        raise SystemExit("debugger capture failed; do not use a stale RAM file")
    match = re.search(r'native status=(\d+) count=(\d+) cycles=(\d+) irqs=(\d+) pc=([0-9a-f]+)', log)
    bases = re.search(r'bases rom=([0-9a-f]+) ram=([0-9a-f]+) guard=([0-9a-f]+)', log)
    if not match or not bases:
        raise SystemExit('missing native diagnostic counters or allocation bases')
    if "vectors restored=1" not in log:
        raise SystemExit("native vector restoration was not verified")
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
               '--out', 'tmp/native-comparison']
    for option, value in zip(['--rom-base', '--ram-base', '--device-base'], bases.groups()):
        command.extend([option, '0x'+value])
    subprocess.run(command, cwd=ROOT, check=True)
    context = (ROOT/'tmp/native-comparison-context.txt').read_text()
    device = (ROOT/'tmp/native-comparison-devices.txt').read_text()
    endpoint = re.search(r'instructions=(\d+) cycles=(\d+) pc=([0-9a-f]+)', context)
    host_irqs = re.search(r'IRQs=(\d+)', device)
    rom_base,ram_base,_ = (int(value,16) for value in bases.groups())
    expected_pc=rom_base+pc if pc<0x40000 else ram_base+pc-0x40000
    if not endpoint or not host_irqs or (int(endpoint[1]), int(endpoint[2]), int(endpoint[3],16), int(host_irqs[1])) != (count,cycles,expected_pc,irqs):
        raise SystemExit('host/native instruction, cycle, PC or IRQ boundary differs')
    reference = (ROOT/'tmp/native-comparison-ram.bin').read_bytes()
    differences = [i for i,(a,b) in enumerate(zip(reference,native)) if a != b]
    if len(reference) != len(native) or differences:
        for i in differences[:32]:
            print(f'{0x40000+i:05x}: host={reference[i]:02x} native={native[i]:02x}')
        raise SystemExit(f'FAIL: {len(differences)} differing RAM bytes')
    print(f'PASS: all {len(native)} RAM bytes match at {count} instructions, {cycles} cycles, {irqs} IRQs')

if __name__ == '__main__':
    main()
