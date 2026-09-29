#!/usr/bin/env python3
"""Extract our assembled sentinel body for the host-only CPU differential test."""
from pathlib import Path
import argparse
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--elf', type=Path, default=root/'amiga/out/Pokeri.elf')
elf = parser.parse_args().elf
symbols = subprocess.check_output(['m68k-amiga-elf-objdump', '-t', str(elf)], text=True)
names = ('nativeShortSerialGuard','nativeShortSerialPost','nativeShortSerialBit','nativeShortAbsoluteGuard','nativeShortAbsoluteRead','presentationTickFrame','nativeUserTrapEnabled','nativeVirtualUsp','nativeVirtualSsp','nativeStackSwitchEnabled','nativeShortSentinelRead', 'nativeShortDone', 'nativeShortSentinelGuard',
         'nativeShortAdmitted', 'nativeShortDecline', 'nativeRomBegin', 'nativeRomEnd',
         'nativeRamBegin', 'nativeRamEnd', 'nativeShortControlGuard', 'nativeShortControlRead',
         'nativeShortLengthDone', 'nativeRegisters', 'nativeShortPiaGuard', 'nativeShortPiaRead',
         'nativeShortPiaWrite','nativeShortPiaReadValue','nativeShortIoGuard','nativeShortIoRead',
         'nativeShortIoWriteValue','nativeShortIoReadValue','nativeTrapGuard','nativeTrapAdmitted',
         'nativeTrapDecline','nativeShortTrapRead','nativeShortTraps','nativeShortVideoGuard','nativeShortVideoWrite','nativeShortVideoWriteValue','nativeShortAddressWrite','nativeVideoSelector','nativeFeedInlineCount','nativeFeedHeaderGrant')
addresses = {line.split()[-1]: int(line.split()[0], 16) for line in symbols.splitlines()
             if line.split() and line.split()[-1] in names}
# Place ABI stubs away from synthetic guest code even as the linked image grows.
helpers = ('nativeShortPiaWrite','nativeShortPiaReadValue','nativeShortIoWriteValue',
           'nativeShortIoReadValue','nativeShortVideoWriteValue')
helper_relocations = {addresses[n]: 0xd0000+i*256 for i,n in enumerate(helpers)}
for n in helpers:
    addresses[n] = helper_relocations[addresses[n]]
data = elf.read_bytes()
assert data[:6] == b'\x7fELF\x01\x02', 'expected big-endian ELF32'
header = struct.unpack_from('>HHIIIIIHHHHHH', data, 16)
section_offset, section_size, count = header[5], header[10], header[11]
def extract(first, last, filename):
    begin, end = addresses[first], addresses[last]
    for i in range(count):
        _, kind, flags, address, offset, size, *_ = struct.unpack_from('>10I', data, section_offset+i*section_size)
        if kind == 1 and flags & 4 and address <= begin < end <= address+size:
            code = data[offset+begin-address:offset+end-address]
            break
    else:
        raise AssertionError('sentinel code section missing')
    for old,new in helper_relocations.items():
        code=code.replace(struct.pack(">HI",0x4eb9,old),struct.pack(">HI",0x4eb9,new))
    assert len(code) < 2048
    path = root / 'tmp' / filename
    path.write_bytes(code)
    return str(path)

flags = extract('nativeShortSentinelRead', 'nativeShortDone', 'native-short-sentinel-code.bin')
guard = extract('nativeShortSentinelGuard', 'nativeShortAdmitted', 'native-short-guard-code.bin')
decline = addresses['nativeShortDecline'] - addresses['nativeShortSentinelGuard']
subprocess.run([str(root/'build/native-short-flags-test'), flags, guard, str(decline)] +
               [str(addresses[n]) for n in ('nativeRomBegin','nativeRomEnd','nativeRamBegin','nativeRamEnd')] +
               [extract('nativeShortControlGuard','nativeShortDone','native-short-control-code.bin')] +
               [str(addresses[n]-addresses['nativeShortControlGuard']) for n in
                ('nativeShortAdmitted','nativeShortDecline','nativeShortControlRead','nativeShortLengthDone')] +
               [str(addresses['nativeRegisters']+68)] +
               [extract('nativeShortPiaGuard','nativeShortControlGuard','native-short-pia-guard.bin')] +
               [str(addresses[n]-addresses['nativeShortPiaGuard']) for n in ('nativeShortAdmitted','nativeShortDecline')] +
               [extract('nativeShortPiaRead','nativeShortControlRead','native-short-pia-body.bin'),
                str(addresses['nativeShortLengthDone']-addresses['nativeShortPiaRead']),
                str(addresses['nativeShortPiaWrite']),str(addresses['nativeShortPiaReadValue'])] +
               [extract('nativeShortIoGuard','nativeShortPiaGuard','native-short-io-guard.bin')] +
               [str(addresses[n]-addresses['nativeShortIoGuard']) for n in ('nativeShortAdmitted','nativeShortDecline')] +
               [extract('nativeShortIoRead','nativeShortPiaRead','native-short-io-body.bin'),
                str(addresses['nativeShortDone']-addresses['nativeShortIoRead']),
                str(addresses['nativeShortIoWriteValue']),str(addresses['nativeShortIoReadValue'])] +
               [extract('nativeTrapGuard','nativeTrapAdmitted','native-short-trap-guard.bin')] +
               [str(addresses[n]-addresses['nativeTrapGuard']) for n in ('nativeTrapAdmitted','nativeTrapDecline')] +
               [extract('nativeShortTrapRead','nativeTrapDecline','native-short-trap-body.bin'),
                str(addresses['nativeShortLengthDone']-addresses['nativeShortTrapRead']),
                str(addresses['nativeShortTraps'])] +
               [extract('nativeShortVideoGuard','nativeShortIoGuard','native-short-video-guard.bin')] +
               [str(addresses[n]-addresses['nativeShortVideoGuard']) for n in ('nativeShortAdmitted','nativeShortDecline')] +
               [extract('nativeShortVideoWrite','nativeShortIoRead','native-short-video-body.bin'),
                str(addresses['nativeShortDone']-addresses['nativeShortVideoWrite']),
                str(addresses['nativeShortVideoWriteValue']),
                str(addresses['nativeShortAddressWrite']-addresses['nativeShortVideoWrite']),
                str(addresses['nativeVideoSelector']),str(addresses['nativeFeedInlineCount']),
                str(addresses['nativeFeedHeaderGrant'])]+
               [str(addresses[n]) for n in ('nativeVirtualUsp','nativeVirtualSsp','nativeStackSwitchEnabled','nativeUserTrapEnabled','presentationTickFrame')]+
               [extract('nativeShortAbsoluteGuard','nativeShortIoGuard','native-short-absolute-guard.bin')]+
               [str(addresses[n]-addresses['nativeShortAbsoluteGuard']) for n in ('nativeShortAdmitted','nativeShortDecline')]+
               [extract('nativeShortAbsoluteRead','nativeShortIoRead','native-short-absolute-body.bin'),
                str(addresses['nativeShortDone']-addresses['nativeShortAbsoluteRead']),
                str(addresses['nativeShortIoReadValue']),str(addresses['nativeShortVideoWriteValue'])]+
               [str(addresses['nativeShortSerialGuard']-addresses['nativeShortIoGuard']),
                str(addresses['nativeShortSerialPost']-addresses['nativeShortIoRead']),
                str(addresses['nativeShortSerialBit']-addresses['nativeShortIoRead'])], check=True)
