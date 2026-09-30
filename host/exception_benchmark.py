#!/usr/bin/env python3
"""Read authored WHDLoad exception benchmark records; no ROM inputs."""
import argparse
import statistics
import struct
from pathlib import Path

NAMES = ("Line-A", "TRAP0", "Privilege", "Level 2", "Level 3")


def read(path):
    data = Path(path).read_bytes()
    if len(data) != 264 or data[:8] != b"PKEX0001":
        raise ValueError(f"{path}: invalid benchmark format")
    hz, count, trials, kinds = struct.unpack_from(">4I", data, 8)
    if (hz, count, trials, kinds) != (709379, 128, 4, 5):
        raise ValueError(f"{path}: unsupported benchmark settings")
    rows = list(struct.iter_unpack(">3I", data[24:]))
    if any(not (0 < base < elapsed < 65536) or delivered != count
           for base, elapsed, delivered in rows):
        raise ValueError(f"{path}: invalid timer/count result")
    return {name: [(elapsed-base)*1e6/hz/count
                   for base, elapsed, _ in rows[i*trials:(i+1)*trials]]
            for i, name in enumerate(NAMES)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixed", nargs="+", required=True, type=Path)
    parser.add_argument("--moved", nargs="+", required=True, type=Path)
    args = parser.parse_args()
    samples = {}
    for mode in ("fixed", "moved"):
        samples[mode] = {name: [] for name in NAMES}
        for path in getattr(args, mode):
            for name, values in read(path).items():
                samples[mode][name].extend(values)
    print("Microseconds per exception, paired loop baseline subtracted.")
    print("Type          Fixed median    Moved median    Added    Fixed range    Moved range")
    for name in NAMES:
        f, m = samples["fixed"][name], samples["moved"][name]
        fm, mm = statistics.median(f), statistics.median(m)
        print(f"{name:12} {fm:12.3f} {mm:15.3f} {mm-fm:8.3f} "
              f"{min(f):.3f}–{max(f):.3f} {min(m):.3f}–{max(m):.3f}")


if __name__ == "__main__":
    main()
