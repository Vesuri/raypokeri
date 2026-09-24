// Runtime shims for building Pokeri with m68k-amiga-elf-gcc (-nostdlib).
// Derived from the dA JoRMaS template's GCCRuntime.cpp (via Rescue on Fractalus), modified:
//   - No demo-timeline dependency. Pokeri registers its VBI server with Exec;
//     Native.cpp owns a temporary shim that chains Exec's level-3 handler.

#include <proto/exec.h>
#include <exec/execbase.h>
#include <exec/memory.h>

// SysBase: defined here and initialised before main() via an init-array ctor
// (the gcc8 crt runs __init_array_start.. before calling main()).
struct ExecBase* SysBase = 0;
__attribute__((constructor)) static void initSysBase() { SysBase = *(struct ExecBase**)4UL; }

// GfxBase: the proto/graphics.h inline calls (LoadView/WaitTOF/...) use this global
// library base.  Set by Pokeri's ctor after OpenLibrary("graphics.library").
struct GfxBase* GfxBase = 0;

// ---- C++ heap via AllocMem --------------------------------------------------
// Track allocations so a fatal service-stack escape can also reclaim temporary
// containers whose destructors could not run. Release the remainder only after
// Pokeri has restored hardware/OS state and its normal destructors have run.
struct HeapAllocation { HeapAllocation *previous, *next; unsigned long size; };
static HeapAllocation *heapHead;
void* operator new(unsigned long n) {
    if(n > ~0UL-sizeof(HeapAllocation)) return nullptr;
    auto *p=(HeapAllocation*)AllocMem(n+sizeof(HeapAllocation),MEMF_ANY|MEMF_CLEAR);
    if(!p)return nullptr;
    p->size=n+sizeof(HeapAllocation);p->previous=nullptr;p->next=heapHead;
    if(heapHead)heapHead->previous=p;
    heapHead=p;return p+1;
}
void* operator new[](unsigned long n) { return operator new(n); }
void operator delete(void *data) {
    if(!data)return;
    auto *p=(HeapAllocation*)data-1;
    if(p->previous)p->previous->next=p->next;else heapHead=p->next;
    if(p->next)p->next->previous=p->previous;
    FreeMem(p,p->size);
}
void operator delete[](void *p) { operator delete(p); }
void operator delete(void *p,unsigned long) { operator delete(p); }
void operator delete[](void *p,unsigned long) { operator delete(p); }
extern "C" void pokeriReleaseHeap() { while(heapHead)operator delete(heapHead+1); }

extern "C" void __cxa_pure_virtual() { for (;;) ; }

// ---- minimal libc bits (-nostdlib) -----------------------------------------
extern "C" int  abs(int x)   { return x < 0 ? -x : x; }
extern "C" long labs(long x) { return x < 0 ? -x : x; }
extern "C" void qsort(void* base, unsigned long n, unsigned long size,
                      int (*cmp)(const void*, const void*)) {
    char* a = (char*)base;                               // insertion sort (small sets)
    for (unsigned long i = 1; i < n; i++)
        for (unsigned long j = i; j > 0 && cmp(a + (j - 1) * size, a + j * size) > 0; j--) {
            char* x = a + (j - 1) * size; char* y = a + j * size;
            for (unsigned long k = 0; k < size; k++) { char t = x[k]; x[k] = y[k]; y[k] = t; }
        }
}
