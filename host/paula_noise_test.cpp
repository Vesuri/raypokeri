#include "../src/platform/amiga/PaulaNoise.h"
#include "../src/platform/amiga/PaulaPeriods.h"
#include "../src/board/Board.h"
#include <array>
#include <cassert>
#include <cstdio>
int main(){
    alignas(4) std::array<uint8_t,pokeri::PaulaNoise::Bytes+8> guarded;
    guarded.fill(42);auto *data=guarded.data()+4;
    pokeri::PaulaNoise source;pokeri::Ay38912 reference;reference.lfsr=source.lfsr;
    auto check=[&](unsigned start,unsigned count){
        for(unsigned i=start;i<start+count;++i){
            const unsigned sample=(reference.lfsr&1)?127:129;
            assert(data[i]==sample);
            assert(data[pokeri::PaulaNoise::Length+i]==((i&1)?129:sample));
            assert(data[2*pokeri::PaulaNoise::Length+i]==((i&4)?129:sample));
            assert(data[3*pokeri::PaulaNoise::Length+i]==((i&16)?129:sample));
            assert(data[4*pokeri::PaulaNoise::Length+i]==((i&64)?129:sample));
            reference.clockStep();reference.clockStep();
        }
        assert(source.lfsr==reference.lfsr && guarded.front()==42 && guarded.back()==42);
    };
    source.prepare(data);check(0,pokeri::PaulaNoise::Length);
    for(unsigned frame=0;frame<2300;++frame){
        auto before=guarded;unsigned start=source.cursor;
        source.refresh(data);check(start,pokeri::PaulaNoise::Refresh);
        for(unsigned wave=0;wave<pokeri::PaulaNoise::Waves;++wave)for(unsigned i=0;i<pokeri::PaulaNoise::Length;++i)
            if(i<start || i>=start+pokeri::PaulaNoise::Refresh)
                assert(data[wave*pokeri::PaulaNoise::Length+i]==before[4+wave*pokeri::PaulaNoise::Length+i]);
    }
    uint16_t pure[4096];pokeri::paulaPeriods(pure);
    for(unsigned p=0;p<4096;++p){
        unsigned samples=2;
        for(unsigned candidate:{8u,32u,128u})
            if(uint64_t(3546895)*16*(p?p:1)/(1000000*candidate)>=124)samples=candidate;
        assert(pokeri::paulaMixedSamples(p)==samples);
        unsigned expected=uint64_t(3546895)*16*(p?p:1)/(1000000*samples);
        if(expected<124)expected=124;
        assert(pokeri::paulaMixedPeriod(p,pure[p])==expected);
    }
    // Audible regression: the period-568 effect formerly updated noise at
    // only 220 Hz. Preserve pitch but lift its noise sample rate above 14 kHz.
    assert(pokeri::paulaMixedSamples(568)==128);
    assert(3546895/pokeri::paulaMixedPeriod(568,pure[568])>14000);
    uint16_t periods[32];pokeri::paulaNoisePeriods(periods);
    for(unsigned p=0;p<32;++p){unsigned expected=uint64_t(3546895)*16*(p?p:1)/1000000;
        if(expected<124)expected=124;assert(periods[p]==expected);}
    for(unsigned p=124;p<65536;++p){unsigned bytes=pokeri::paulaSlice(p);
        assert(bytes>=2 && bytes<=256 && !(bytes&(bytes-1)));
        assert(bytes==2 || uint64_t(bytes)*p<=28375);
    }
    puts("PASS: shared AY noise bits, fixed tone gates, bounded refresh/wrap, untouched bytes, all periods and DMA slice bounds");
}
