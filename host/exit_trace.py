#!/usr/bin/env python3
"""Find the post-WHDLoad D6/D7 marker in FS-UAE instruction captures.

Reports PAL fields from the first recorded field, NOT from the GDB breakpoint.
The capture-start boundary and DOS helper-launch overhead must be qualified
separately before this can be called a complete save-exit duration.
"""
import argparse
import json
import mmap
import struct

MARKER = (0x504F4B21, 0x45584954)


def inspect(data):
    offset = 0

    def take(size):
        nonlocal offset
        if size < 0 or size > len(data) - offset:
            raise ValueError('truncated trace')
        start = offset
        offset += size
        return start

    def word():
        return struct.unpack_from('<I', data, take(4))[0]

    fields, sections = word(), word()
    take(sections * 4 + 16)
    for _ in range(3):
        take(word())
    clock, cycle_unit = word(), word()
    if not fields or not clock or not cycle_unit:
        raise ValueError('empty trace or invalid clock header')
    markers = []
    total_cycles = 0
    maximum_accounting_error = 0
    for field in range(fields):
        size = word()
        if size != 520:
            raise ValueError('invalid custom register block')
        take(size)
        size = word()
        if size not in (0, 1024):
            raise ValueError('invalid AGA register block')
        take(size)
        for _ in range(2):
            size, count = word(), word()
            take(size * count)
        cycles, idle, count = word(), word(), word()
        if not cycles:
            raise ValueError('zero-cycle field')
        end = offset + 4 * count
        if end > len(data):
            raise ValueError('truncated instruction block')
        accounted = 0
        while offset < end:
            while True:
                value = word()
                if value >= 0xffff0000:
                    break
            registers = struct.unpack_from('<17I', data, take(68))
            if registers[6:8] == MARKER:
                markers.append({'field': field, 'cycle_in_field': accounted,
                                'field_cycles': cycles,
                                'pal_fields_from_capture': field + accounted / cycles})
            accounted += 0xffffffff - value
        if offset != end:
            raise ValueError('instruction block boundary mismatch')
        error = abs(accounted - cycles)
        maximum_accounting_error = max(maximum_accounting_error, error)
        if error > 100 + cycles // 1000:
            raise ValueError('instruction cycles do not account for the field')
        size = word()
        word()  # screenshot format
        take(size)
        total_cycles += cycles
    if offset != len(data):
        raise ValueError('trailing trace bytes')
    return dict(fields=fields, clock=clock, cycle_unit=cycle_unit,
                total_cycles=total_cycles, markers=markers,
                maximum_accounting_error=maximum_accounting_error)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', nargs='+')
    options = parser.parse_args()
    field_offset = 0
    results = []
    for name in options.trace:
        with open(name, 'rb') as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as data:
            result = inspect(data)
        result['file'] = name
        for marker in result['markers']:
            marker['pal_fields_from_capture'] += field_offset
        field_offset += result['fields']
        results.append(result)
    print(json.dumps(results, indent=2))
    if not any(result['markers'] for result in results):
        parser.exit(1, 'post-return marker not found; exit measurement is incomplete\n')


if __name__ == '__main__':
    main()
