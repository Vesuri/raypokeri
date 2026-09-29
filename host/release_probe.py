#!/usr/bin/env python3
"""Resolve read-only release counter probes against the exact executable."""
import argparse
from pathlib import Path
from native_trace import load_elf, card_counters, text_word


def prepare(elf, template):
    if '@CARD_' not in template:
        return template
    symbols, _, _, _ = load_elf(elf)
    candidates = [(a, n) for a, n, name in symbols if name == 'CardBackCache::command']
    if len(candidates) != 1:
        raise ValueError('expected one card command function')
    address, size = candidates[0]
    sites = card_counters(elf, address, size)
    for field, name in (('starts', 'BEGIN'), ('hits', 'HIT')):
        pc = sites[field]
        instruction = (text_word(elf, pc) << 16) | text_word(elf, pc + 2)
        template = template.replace(f'@CARD_{name}_OFFSET@', hex(pc - address))
        template = template.replace(f'@CARD_{name}_INSTRUCTION@', hex(instruction))
    if '@CARD_' in template:
        raise ValueError('unresolved card probe marker')
    return template


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', required=True, type=Path)
    parser.add_argument('--template', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    args.out.write_text(prepare(args.elf, args.template.read_text()))
