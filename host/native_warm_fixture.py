#!/usr/bin/env python3
"""Create a local zero-credit accounting fixture or seed a new native drive."""
from pathlib import Path
import argparse
import struct
import subprocess
import zlib

ROOT=Path(__file__).resolve().parents[1]

def validate(data):
    if len(data)!=940 or data[:8]!=b'PKAC0001' or zlib.crc32(data[:-4])!=struct.unpack('>I',data[-4:])[0]:
        raise ValueError('invalid accounting fixture')

def install(seed,drive):
    destination=drive/'accounting.bin'
    if destination.exists():return False # Never replace a player's existing save.
    data=seed.read_bytes();validate(data)
    drive.mkdir(parents=True,exist_ok=True)
    # Exclusive creation also protects a save created since the existence check.
    with destination.open('xb') as f:f.write(data)
    return True

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--create',type=Path)
    parser.add_argument('--seed',type=Path)
    parser.add_argument('--drive',type=Path)
    args=parser.parse_args()
    if args.create:
        target=args.create.resolve()
        if ROOT/'tmp' not in target.parents:parser.error('generated fixture must be under repo tmp/')
        if target.exists():parser.error('fixture already exists; choose a new path')
        subprocess.run(['make','harness'],cwd=ROOT,check=True)
        from accounting_check import COMMON
        relative=str(target.relative_to(ROOT))
        subprocess.run(COMMON+['--auto-setup','--accounting-ram',relative,'--ms','1000',
                       '--out','tmp/native-warm-fixture'],cwd=ROOT,check=True)
        validate(target.read_bytes())
        print('Created zero-credit fixture:',target)
    elif args.seed and args.drive:
        print('Installed accounting fixture' if install(args.seed,args.drive) else 'Kept existing accounting save')
    else:parser.error('use --create PATH or --seed PATH --drive DIRECTORY')

if __name__=='__main__':main()
