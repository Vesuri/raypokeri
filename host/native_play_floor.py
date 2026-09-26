#!/usr/bin/env python3
"""Report conservative original-work bounds from opt-in native input snapshots.

This is a workload calibration, not a clock-policy change. Each completed normal
main-loop pass includes 7424 SUBQ.W/BNE iterations, 103934 reference cycles.
Discard one pass per interval to exclude a partial first iteration. All other
original instructions are deliberately omitted, then all nominal hook charges
are subtracted, so the resulting reference/guest ratio is a lower bound.
"""
import argparse
from pathlib import Path
import struct

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("capture", type=Path)
a = p.parse_args()
raw = a.capture.read_bytes()
if len(raw) != 26*20:
    p.error("expected all 26 five-word snapshots")
rows = list(struct.iter_unpack(">5I", raw))
phases = [(0, 3, "ready/coin"), (3, 5, "deal"), (5, 11, "hold"),
          (11, 13, "draw"), (13, 15, "double"), (15, 17, "big"),
          (17, 21, "panel/coin"), (21, 23, "door open"), (23, 25, "door close/idle")]
print("| Phase | Board s | PAL s | Complete loops (conservative) | Guest cycles | Hook cycles | Reference/guest lower bound |")
print("|---|---:|---:|---:|---:|---:|---:|")
bounds = []
for lo, hi, name in phases:
    c, f, g, h, loops = [((b-a) & 0xffffffff) for a, b in zip(rows[lo], rows[hi])]
    if not rows[lo][0] or not rows[hi][0] or not g or c >= 0x80000000:
        p.error(f"missing or invalid {name} interval")
    loops = max(loops-1, 0)
    lower = max(loops*103934-h, 0)/g
    print(f"| {name} | {c/8000000:.3f} | {f/50:.2f} | {loops:,} | {g:,} | {h:,} | {lower:.3f} |")
    bounds.append(lower)
print(f"\nMinimum measured lower bound: {min(bounds):.3f}; with 12.5% headroom: {min(bounds)*.875:.3f}.")
print("Short intervals are sensitive to conservative partial-pass exclusion. These workloads do not establish untested paths or physical board timing.")
