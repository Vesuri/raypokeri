#ifndef POKERI_DISPLAY_GEOMETRY_H
#define POKERI_DISPLAY_GEOMETRY_H
namespace pokeri {
// Interleaved display accesses fetch one group every two memory cycles.
// Retain the final whole fetch for the ROM's odd-width window. This edge
// behavior is inferred from artwork/footage; see docs/rom-set.md.
struct InterleavedWindow {
    int x;
    unsigned width;
    InterleavedWindow(unsigned hwr,unsigned hdr,unsigned pixelsPerCycle) {
        unsigned cycles=(hwr&255)+1;
        int delay=(cycles&1)?2:0;
        x=(int(hwr>>8)-int(hdr>>8)+delay)*int(pixelsPerCycle);
        width=((cycles+1)&~1u)*pixelsPerCycle;
    }
};
}
#endif
