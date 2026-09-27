// Synthetic ownership tests: no ROM, pixels or custom-chip register emulation.
#include "../src/platform/FrameSwap.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
    using pokeri::FrameSwap;
    for(unsigned line=0;line<512;++line)for(bool pending:{false,true})
        assert(FrameSwap::armWindow(line,pending)==(!pending && line>=8 && line<300));
    for(uint32_t first:{0u,0xfffffffdu})for(unsigned initial:{0u,1u}){
        FrameSwap s;s.frame=first;s.front=initial;
        assert(!s.arm());s.pending=initial;assert(!s.arm());s.pending=-1;
        for(unsigned n=0;n<100;++n){
            unsigned old=s.front,back=old^1;s.pending=back;
            // A busy blitter/unsafe beam leaves the completed old front owned.
            for(unsigned missed=0;missed<(n&3);++missed){assert(!s.vblank());assert(s.front==old && s.pending==int(back));}
            assert(s.arm());assert(!s.arm());assert(s.front==old && s.pending==int(back));
            assert(s.vblank());assert(s.front==back && s.pending==-1 && s.armed==-1);
        }
    }
    FrameSwap s;unsigned swaps=0;
    for(unsigned n=0;n<100;++n){s.pending=s.front^1;assert(s.arm());swaps+=s.vblank();}
    assert(swaps==100); // Arming between VBIs permits one flip per PAL frame.
    puts("PASS: frame ownership, delayed readiness, invalid arms, counter wrap and one-swap-per-frame scheduling");
}
