#!/usr/bin/env python3
"""Exercise the actual allocator/factory bodies with a checked host Exec mock.

Select the allocation-only portions before their hardware/assembly methods; no
allocation implementation is copied into the test. The mock rejects wrong-size
and duplicate frees and fails each allocation in turn. Native ABI/DMA lifetime
still require emulator tests.
"""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
out = root / 'build'
out.mkdir(exist_ok=True)
framework = root / 'src/platform/amiga/framework'

def portion(path, start, end):
    source = path.read_text()
    return source[source.index(start):source.index(end)]

runtime = portion(root / 'src/platform/amiga/GCCRuntime.cpp',
                  'struct HeapAllocation', 'extern "C" void __cxa_pure_virtual')
# Exec/m68k only needs word alignment; host operator new promises 16-byte alignment on this host.
# Pad the tracking header solely for the host ABI used by the sanitizer.
runtime = runtime.replace('struct HeapAllocation {', 'struct alignas(16) HeapAllocation {')
copper = portion(framework / 'CopperList.cpp', 'CopperList::CopperList()', '// GCC + ASSEMBLER')
bitmap = portion(framework / 'Bitmap.cpp', 'Bitmap::Bitmap(', '// GCC + ASSEMBLER')
# The address comparison only determines whether a bitmap is in Chip RAM; use
# pointer-width arithmetic on the host without altering allocation logic.
bitmap = bitmap.replace('(uint32_t)data <', '(uintptr_t)data <')
sprite = (framework / 'Sprite.cpp').read_text().split('Sprite::Sprite(', 1)[1]
sprite = 'Sprite::Sprite(' + sprite
source = r'''
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _UTIL_H
struct Polygon;
#define MEMF_ANY 0
#define MEMF_CLEAR 1
#define MEMF_CHIP 2
static void check(bool b){if(!b){fputs("memory audit assertion failed\n",stderr);abort();}}
struct Block {void *p; unsigned long size;};
static Block blocks[4096];
static unsigned calls, failAt, outstanding;
static void *AllocMem(unsigned long n,unsigned long flags){
    if(++calls==failAt)return nullptr;
    void *p=malloc(n?n:1);check(p!=nullptr);
    if(flags & MEMF_CLEAR)memset(p,0,n);
    for(auto &b:blocks)if(!b.p){b={p,n};++outstanding;return p;}
    abort();
}
static void FreeMem(void *p,unsigned long size){
    for(auto &b:blocks)if(b.p==p){check(size==b.size);b.p=nullptr;--outstanding;free(p);return;}
    abort(); // unknown or already freed block
}
#include "src/platform/amiga/framework/CopperList.h"
#include "src/platform/amiga/framework/Bitmap.h"
#include "src/platform/amiga/framework/Sprite.h"
'''
source += runtime + copper + bitmap + sprite
source += r'''
static void reset(unsigned failure=0){check(!outstanding && !heapHead);calls=0;failAt=failure;}
int main(){
    // Each two-stage factory: Chip allocation fails, owner allocation fails,
    // then success. Raw Chip storage must never escape the heap's ownership.
    for(unsigned failure=1;failure<=3;++failure){
        reset(failure);auto *c=CopperList::allocate(48);check(bool(c)==(failure==3));delete c;reset();
        reset(failure);auto *s=Sprite::allocate(100);check(bool(s)==(failure==3));delete s;reset();
        reset(failure);auto *b=Bitmap::allocate(87,100,4,true);check(bool(b)==(failure==3));
        if(b){check(b->widthInBytes==12 && b->dataSize()==4800);memset(b->data,0x5a,b->dataSize());}
        delete b;reset();
    }
    check(!CopperList::allocate(0) && !CopperList::allocate(0x40000000));reset();
    check(!Bitmap::allocate(0,10,4,true) && !Bitmap::allocate(87,100,4,true,80));
    check(!Bitmap::allocate(65535,65535,4,true));reset();
    for(unsigned failure=1;failure<=3;++failure){
        auto *b=Bitmap::allocate(87,100,4,true);check(b!=nullptr);
        memset(b->data,0xff,b->dataSize());calls=0;failAt=failure;
        auto *mask=Bitmap::generateMask(*b);check(bool(mask)==(failure==3));
        if(mask)for(unsigned i=0;i<mask->dataSize();++i)check(((uint8_t*)mask->data)[i]==255);
        delete mask;delete b;reset();
    }
    // Heap unlinking in all positions, null deletes, overflow and failed new.
    void *a=operator new(16),*b=operator new(32),*c=operator new(64);
    operator delete(b);operator delete(c);operator delete(a);operator delete(nullptr);reset();
    check(!operator new(~0UL));reset(1);check(!operator new(16));reset();
    // Abandoned temporaries after a fatal service-stack escape, followed by
    // a harmless repeated sweep.
    operator new(16);operator new[](32);pokeriAllocateUninitialized(128);
    pokeriReleaseHeap();pokeriReleaseHeap();reset();
    puts("PASS Amiga heap sizes/unlink/failure/sweep; Copper/bitmap/sprite allocation failures; padded bitmap/mask bounds");
}
'''
path = out / 'amiga-memory-test.cpp'
path.write_text(source)
exe = out / 'amiga-memory-test'
subprocess.run([os.environ.get('CXX', 'clang++'), '-std=c++14', '-fcheck-new',
                '-fno-exceptions', '-fsanitize=address,undefined', '-g',
                '-I' + str(root), str(path), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True, env=dict(os.environ, UBSAN_OPTIONS='halt_on_error=1'))
