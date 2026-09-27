#ifndef POKERI_DISPLAY_GEOMETRY_H
#define POKERI_DISPLAY_GEOMETRY_H
namespace pokeri {
// Pokeri draws meaningful header pixels through x=607. Present the complete
// programmed 152-word row for this ROM mode; keep its timing registers intact.
// This is the user's approved viewport correction, not a new physical timing
// claim. Other display modes retain their nominal HDW calculation.
inline unsigned presentationWidth(unsigned hdr,unsigned memoryWidth,unsigned pixelsPerCycle){
    return hdr==0x0947 && memoryWidth==152 && pixelsPerCycle==8 ? 608 :
        ((hdr&255)+1)*pixelsPerCycle;
}
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
