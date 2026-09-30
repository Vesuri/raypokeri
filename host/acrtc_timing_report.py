#!/usr/bin/env python3
"""Summarize isolated ACRTC timing events; never call synthetic rates calibrated."""
import argparse
import collections
import json
import re
from pathlib import Path


def read(path):
    result = {'services': [], 'ay': [], 'ready_cycle': None, 'video_irqs': 0,
              'watchdog_resets': 0, 'timing': []}
    with path.open() as stream:
        for line in stream:
            if line.startswith('acrtc-irq '):
                result['video_irqs'] += 1
            elif line.startswith('acrtc-service '):
                values = dict((key, int(value)) for key, value in re.findall(r'(\w+)=(\d+)', line))
                result['services'].append(values)
            elif line.startswith('AY register='):
                match = re.search(r'register=(\d+) value=([0-9a-f]+).* cycle=(\d+)', line)
                if not match:
                    raise ValueError('malformed AY record')
                result['ay'].append((int(match[1]), int(match[2], 16), int(match[3])))
            elif line.startswith('setup stage=8 '):
                result['ready_cycle'] = int(re.search(r'cycles=(\d+)', line)[1])
            elif line.startswith('timed '):
                result['timing'].append(line.strip())
            elif 'watchdog CPU reset' in line:
                result['watchdog_resets'] += 1
    return result


def report(data, reference=None):
    services = data['services']
    result = {key: data[key] for key in ('ready_cycle', 'video_irqs', 'watchdog_resets', 'timing')}
    result['completed_video_services'] = len(services)
    result['words_per_service_histogram'] = dict(sorted(collections.Counter(s['words'] for s in services).items()))
    result['words_per_service_mean'] = sum(s['words'] for s in services) / len(services) if services else None
    result['ay_writes'] = len(data['ay'])
    if reference:
        same = [(r, v) for r, v, _ in data['ay']] == [(r, v) for r, v, _ in reference['ay']]
        result['same_ay_register_value_sequence'] = same
        if same and data['ay']:
            shifts = [a[2] - b[2] for a, b in zip(data['ay'], reference['ay'])]
            result['ay_cycle_shift_range'] = [min(shifts), max(shifts)]
            # Shift is board-time displacement, NOT native late-write latency.
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('events', nargs='+', type=Path)
    parser.add_argument('--reference', type=Path)
    args = parser.parse_args()
    reference = read(args.reference) if args.reference else None
    print(json.dumps({str(p): report(read(p), reference) for p in args.events}, indent=2))


if __name__ == '__main__':
    main()
