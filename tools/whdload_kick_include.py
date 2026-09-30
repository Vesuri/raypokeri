#!/usr/bin/env python3
"""Generate a local kick31 include with an optional ws_DontCache symbol.

The shared SDK is never edited. The default header word remains zero; a slave
may define slv_DontCache as its pattern label before including kick31.s.
"""
from pathlib import Path
import argparse


def adapt(source):
    original = '\t\tdc.w\t0\t\t\t;ws_DontCache'
    if source.count(original) != 1:
        raise ValueError('unsupported kick31 ws_DontCache header; inspect SDK change')
    replacement = ('\tIFD slv_DontCache\n'
                   '\t\tdc.w slv_DontCache-slv_base\t;ws_DontCache\n'
                   '\tELSE\n' + original + '\n\tENDC')
    return source.replace(original, replacement)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    result = adapt(args.source.read_text())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(result)

if __name__ == '__main__':
    main()
